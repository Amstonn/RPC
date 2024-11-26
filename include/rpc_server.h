#ifndef RPCSERVER
#define RPCSERVER
#include<thread>
#include<mutex>
#include <boost/asio.hpp>
#include <iostream>
#include"connection.h"
#include"io_pool.h"
#include "router.h"

using boost::asio::ip::tcp;
using namespace std;
namespace easy_rpc{
namespace rpc_server{
class RpcServer{
private:
    void do_accept();
    void clean();
    void callback();
    shared_ptr<Connection> conn_; //共享指针 共享一个连接
    shared_ptr<thread> thd_;  //线程指针
    size_t timeout_seconds_; 
    unordered_map<int64_t,shared_ptr<Connection>> connections_; 
    int64_t conn_id = 0; //当前连接id
    mutex mtx_;
    shared_ptr<thread> check_thread_;
    size_t check_seconds_;
    bool stop_check_ = false;
    tcp::acceptor acceptor_;//请求接受器
    io_pool io_pool_; //网络io服务连接池

public:
    RpcServer();
    RpcServer(RpcServer & ) = delete;
    RpcServer & operator = (RpcServer &) = delete; 
    ~RpcServer();
    void run();
    template<ExecMode model = ExecMode::sync,typename Function>
    void register_handler(string const &name,const Function &f);
    template<ExecMode model = ExecMode::sync,typename Function, typename Self>
    void register_handler(string const &name,const Function &f,Self * self);
    void response(int64_t conn_id, string && result);
};
}
};
#endif