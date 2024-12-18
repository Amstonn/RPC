#ifndef RPC_CLIENT_HPP
#define RPC_CLIENT_HPP
#include <string>
#include <deque>
#include <future>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
using boost::asio::ip::tcp;
#include "constvars.h"
#include "client_util.h"

using namespace easy_rpc::rpc_server;

namespace easy_rpc{
    using namespace boost::system;
    class req_result{
    private:
        string_view data_;
    public:
        req_result() = default;
        req_result(string_view data):data_(data){};
        bool success() const{
            return !has_error(data_);
        }

        template<typename T>
        T as(){
            return get_result<T>(data_);
        }

        void as() {
            if(has_error(data_)){
                throw std::logic_error("rpc error");
            }
        }

    };

    class rpc_client:private boost::noncopyable{
    private:
        boost::asio::io_service ios_; //io服务
        tcp::socket socket_;
        boost::asio::io_service::work work_;
        boost::asio::io_service::strand strand_; //确保同一时刻只有一个任务运行在特定的上下文  也保证了任务序列化执行
        std::shared_ptr<std::thread> thd_ = nullptr;
        std::string host_;
        unsigned short port_;
        size_t connect_timeout = 2;
        size_t wait_timeout = 2;
        int reconnect_cnt = -1;
        bool has_connected = false; //是否成功连接
        std::mutex conn_mtx_;
        std::condition_variable conn_cond_;
        boost::asio::deadline_timer deadline_; //一个计时器
        using message_type = std::pair<string_view, std::uint64_t>;
        std::deque<message_type> outbox_; //信息输出队列
        uint64_t req_id = 0; //请求序列号
        std::function<void(boost::system::error_code) > err_cb_; //err处理函数
        std::unordered_map<std::uint64_t , std::shared_ptr<std::promise<req_result>>> future_map_; //相应接收的map
        char head_[HEAD_LEN] = {}; //回复头
        std::vector<char> body_; //回复体

        /*
        @breif 重置超时计时器，超时自动关闭socket连接
        @param timeout 超时的时间
        */
        void reset_deadline_timer(size_t timeout){
            deadline_.expires_from_now(boost::posix_time::seconds((long)timeout));
            deadline_.async_wait([this](const boost::system::error_code& ec){
                if(!ec){
                    socket_.close();//超时关闭socket连接
                }
            });
        }
        /*
        @brief 将要写的消息放入消息队列中
        */
        void write(std::uint64_t, buffer_type&& message){
            size_t size = message.size();
            assert(size > 0 && size < MAX_BUF_LEN);
            //获取message中的字符串
            message_type msg{{message.release(), size}, req_id};
            strand_.post([this,msg]{
                outbox_.emplace_back(std::move(msg));
                if(outbox_.size() > 1) return;
                this->write();//只有一个任务时写
            });
        }
        /*
        @breif 真实的写入函数
        */
        void write(){
            auto msg = outbox_[0];
            size_t write_len = msg.first.length();
            std::array<boost::asio::const_buffer, 3> write_buffers;
            write_buffers[0] = boost::asio::buffer(&write_len, sizeof(int32_t));
            write_buffers[1] = boost::asio::buffer(&msg.second, sizeof(uint64_t));
            write_buffers[2] = boost::asio::buffer((char*)msg.first.data(), write_len);
            boost::asio::async_write(socket_,write_buffers,
                strand_.wrap([this](const boost::system::error_code& ec,const size_t length){
                    ::free((char*)outbox_.front().first.data());
                    outbox_.pop_front();
                    if(ec){
                        if(err_cb_) err_cb_(ec);
                        return;
                     }
                     if(!outbox_.empty()){
                        this->write();
                     }
                })
            );
        }
        /*
        @brief 读取数据
        */
        void do_read(){
            boost::asio::async_read(socket_, boost::asio::buffer(head_),
            [this](const boost::system::error_code & ec, const size_t length){
                if(!socket_.is_open()){
                    if(err_cb_) err_cb_(errc::make_error_code(errc::connection_aborted));
                    return;
                }
                if(!ec){
                    const uint32_t body_len = *((uint32_t*)(head_));
                    auto req_id = *((std::uint64_t*)head_+sizeof(int32_t));
                    if(body_len > 0 && body_len < MAX_BUF_LEN){
                        if(body_.size() < body_len) body_.resize(body_len);
                        read_body(req_id, body_len);
                        return;
                    }
                    if(body_len == 0 || body_len > MAX_BUF_LEN){
                        call_back(req_id, errc::make_error_code(errc::invalid_argument), {});
                        close();
                        return;
                    }
                }else{
                    if(err_cb_) err_cb_(ec);
                    close();
                }
            });
        }
        /*
        @brief 读取数据体
        */
        void read_body(std::uint64_t req_id, size_t body_len){
            boost::asio::async_read(socket_,boost::asio::buffer(body_.data(), body_len),
            [this, req_id, body_len](boost::system::error_code &ec, std::size_t length){
                if(!socket_.is_open()){
                    call_back(req_id,errc::make_error_code(errc::connection_aborted), {});
                    return;
                }
                if(!ec){
                    call_back(req_id, ec, {body_.data(), body_len});
                    do_read();
                }else{
                    call_back(req_id,ec,{});
                }
            });
        }
        /*
        @brief 获取异步的结果
        */
        std::future<req_result> get_future(){
            auto p = std::make_shared<std::promise<req_result>>();
            std::future<req_result> future = p->get_future(); //获取结果
            strand_.post([this, p = std::move(p)]()mutable{
                future_map_.emplace(req_id, std::move(p));
            }); //加入到future_map中，strand_.post保证了任务的序列化执行，减少了锁的使用
            return future;
        }
        /*
        @brief 异步读时的call_back，用于将读取的结果保存
        */
        void call_back(uint64_t req_id, const boost::system::error_code &ec, string_view data){
            if(!ec){
                auto& f = future_map_[req_id];
                f->set_value(req_result{data});
                strand_.post([this,req_id](){
                    future_map_.erase(req_id);
                });
            }
        }

        void close(){
            has_connected = true;
            if(socket_.is_open()){
                boost::system::error_code ignored_ec;
                socket_.shutdown(tcp::socket::shutdown_both, ignored_ec);
                socket_.close(ignored_ec);
            }
        }

    public:
        rpc_client(const std::string & host, unsigned short port):socket_(ios_),work_(ios_),strand_(ios_),deadline_(ios_),
            host_(host),port_(port),body_(INIT_BUF_SIZE){
                thd_ = std::make_shared<std::thread>([this]{
                    ios_.run();
                });
        }
        ~rpc_client(){
            stop();
        }

        void set_connect_timeout(size_t seconds){
            connect_timeout = seconds;
        }

        void set_reconnect_timeout(int reconn_cnt){
            reconnect_cnt = reconn_cnt;
        }

        void set_wait_timeout(size_t seconds){
            wait_timeout = seconds;
        }

        void async_connect(){
            reset_deadline_timer(connect_timeout);
            auto addr = boost::asio::ip::address::from_string(host_);
            socket_.async_connect({addr, port_},[this](const boost::system::error_code& ec){
                if(ec){
                    std::cout << ec.message() << std::endl;
                    has_connected =  false;
                    if(reconnect_cnt == 0) return;
                    //再次重新连接
                    if(reconnect_cnt > 0){
                        reconnect_cnt --;
                    }
                    socket_ = decltype(socket_)(ios_);
                    async_connect();

                }else{
                    has_connected = true;
                    deadline_.cancel();//取消超时断连
                    do_read();
                    conn_cond_.notify_one();
                }
            });
        }


    };


}

#endif