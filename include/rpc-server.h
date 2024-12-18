#ifndef EASY_RPC_SERVER_CPP
#define EASY_RPC_SERVER_CPP

#include <thread>
#include <mutex>
#include "connection.h"
#include "io_pool.h"
#include "router.h"

using boost::asio::ip::tcp;

namespace easy_rpc{
namespace rpc_server{
class RpcServer{
private:
    io_pool io_pool_; //连接池
    tcp::acceptor acceptor_;//连接接受器
    std::shared_ptr<Connection> conn_;//当前连接
    std::shared_ptr<std::thread> thd_;//服务线程
    std::size_t timeout_second_;
    std::unordered_map<int64_t,std::shared_ptr<Connection>> connections_;//已有连接
    int64_t conn_id = 0;
    std::mutex mtx_;
    std::shared_ptr<std::thread> check_thread_;
    size_t __check_seconds_;
    bool stop_check_ = false; //停止标志位

    void do_accept();
    void clean();
    void callback(const std::string &topic, std::string&& result,Connection * conn, bool has_error = false);
public:
    RpcServer(short port, size_t size, size_t timeout_seds = 15, size_t check_seds = 10);
    RpcServer(RpcServer &) = delete;
    RpcServer & operator = (RpcServer &) = delete;
    ~RpcServer();
    void run();
    /*
    @brief 向Router中注册非成员函数
    */
    template<ExecMode model,typename Function>
    void register_handler(std::string const & name, const Function& f){
        Router::get().register_handler<model> (name,f);
    }
    /*
    @brief 向Router中注册成员函数
    */
    template<ExecMode model ,typename Function, typename Self>
    void register_handler(std::string const &name,const Function &f, Self* self){
        Router::get().register_handler<model>(name,f,self);
    }
    void response(int64_t conn_id, std::string && result);
};
}
}
#endif