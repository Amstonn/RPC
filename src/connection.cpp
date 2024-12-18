#include "connection.h"

namespace easy_rpc{
namespace rpc_server{
Connection::Connection(boost::asio::io_service& io_service,std::size_t timeout_seconds):
socket_(io_service),
body_(INIT_BUF_SIZE),
timer_(io_service),
timeout_seconds_(timeout_seconds),
has_closed_(false){}

Connection::~Connection(){
    close(); 
}
/*
@brief 开始一个连接 读取数据
*/
void Connection::start(){
    read_head();
}
/*
@brief 获取socket
*/
tcp::socket & Connection::socket(){
    return socket_;
}
/*
@brief 连接是否关闭
*/
bool Connection::has_closed() const{
    return has_closed_;
}
/*
@brief 异步回复
@param data 回复的内容
*/
void Connection::response(string data){
    int datalen = data.size();
    assert(datalen < MAX_BUF_LEN);
    //创建三个写缓冲区
    array<boost::asio::const_buffer,3> write_buffers;
    write_msg_ = move(data);
    //第一个缓冲区放入数据长度，第二个缓冲区放入请求id作为唯一标识，第三个缓冲区放入回复的信息
    write_buffers[0] = boost::asio::buffer(&datalen,sizeof(uint32_t));
    write_buffers[1] = boost::asio::buffer(&req_id_,sizeof(uint64_t));
    write_buffers[2] = boost::asio::buffer(write_msg_.data(),datalen);
    
    reset_timer();//重置时间
    auto self = this->shared_from_this();//对本对象创建共享指针，防止在异步未执行完之前销毁
    //异步回复
    boost::asio::async_write(
        socket_,write_buffers,
        [this,self](boost::system::error_code ec,std::size_t length){
            //如果有错误则退出
            if(ec){
                cout << ec.value() << " "<<ec.message()<<endl;
                close(); //写入异常 关闭连接
                return;
            }
            cancel_timer();
            if(has_closed()) return;
            //写入有效 读取信息
            if(!ec) read_head();
            else close();//回复后读取异常 关闭连接
        }
    );
}
/*
@brief 设置连接id
*/
void Connection::set_conn_id(int64_t id){
    conn_id = (size_t)id;
}
/*
@brief 获取连接id
*/
int64_t Connection::get_conn_id(){
    return conn_id;
}
/*
@brief 从socket中读取数据头
*/
void Connection::read_head(){
    reset_timer();
    shared_ptr<Connection> self(this->shared_from_this());
    boost::asio::async_read(
        socket_,boost::asio::buffer(head_),
        /*this：指向当前对象的指针。它允许回调函数直接访问当前对象的成员变量和成员函数
        self：是一个 std::shared_ptr，指向当前对象，并通过 shared_from_this() 获得。
        它的作用是保持当前对象的有效性，确保在异步操作执行期间，该对象不会被销毁。*/
        [this,self](boost::system::error_code ec,std::size_t length){
            if(!socket_.is_open()){ //soket已关闭，直接返回
                return;
            }
            if(!ec){
                //没有错误读取数据  [数据长度(4 bytes) 请求id(8 bytes) 数据] head占用12字节
                const uint32_t body_len = *((int*)(head_));//读取数据长度
                req_id_ = *((uint64_t*)(head_ + sizeof(int32_t)));
                if(body_len > 0 && body_len < MAX_BUF_LEN){
                    if(body_.size() < body_len) {
                        //数据数组空间不足则扩展空间
                        body_.resize(body_len);
                    }
                    read_body(body_len);
                    return;
                }
                if(body_len == 0){
                    //空数据包 可能是heartbeat
                    cancel_timer();
                    read_head();
                }else{
                    close();//获取数据长度小于0 异常关闭连接
                }
            }else{
                close(); //读异常 关闭连接
            }
        }
    );
}
/*
@brief 从socket中读取size大小的数据
*/
void Connection::read_body(size_t size){
    auto self(this->shared_from_this());
    boost::asio::async_read(
        socket_,boost::asio::buffer(body_.data(),size),
        [this,self](boost::system::error_code ec,size_t length){
            cancel_timer();
            if(!socket_.is_open()){
                return;
            }
            if(!ec){
                //获取到数据体 创建Router 匹配命令到需要具体调用的服务
                Router & _router = Router::get();
                _router.route(body_.data(),length,this); //结果会自动调用callback返回数据到conn的客户端
            }else{
                return;
            }
        }
    );
}
/*
@brief 重置计时器
*/
void Connection::reset_timer(){
    if(timeout_seconds_ == 0) return;
    auto self(this->shared_from_this());
    timer_.expires_from_now(boost::posix_time::seconds((long)timeout_seconds_));
    timer_.async_wait([this,self](const boost::system::error_code& ec){
        if(has_closed()) return;
        if(ec) return;
        //timeout时间到自动关闭连接
        close();
    });
}
/*
@brief 取消计时
*/
void Connection::cancel_timer(){
    if(timeout_seconds_ == 0) return;
    timer_.cancel();
}
/*
@brief 关闭连接
*/
void Connection::close(){
    has_closed_ = true;
    if(socket_.is_open()){
        boost::system::error_code ignored_ec;
        //关闭读写通道
        socket_.shutdown(tcp::socket::shutdown_both, ignored_ec);
        //完全关闭套接字
        socket_.close(ignored_ec);
    }
}
}
}