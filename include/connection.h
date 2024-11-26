#ifndef CONNECTION
#define CONNECTION

#include<iostream>
#include <memory>
#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include "constvars.h"

using namespace std;
using boost::asio::ip::tcp;

namespace easy_rpc{
namespace rpc_server{
/*
@brief 连接类 继承自enable_shared_from_this<Connection> 表示该类允许将自身生命周期交由共享指针管理
*/
class Connection : public enable_shared_from_this<Connection>{
private:
    tcp::socket socket_;
    char head_[HEAD_LEN];
    vector<char> body_;
    uint64_t req_id_;
    string write_msg_;
    boost::asio::deadline_timer timer_;
    size_t conn_id=0;
    atomic_bool has_closed_;//多线程环境下安全处理bool值

    void read_head();
    void read_body();
    void reset_timer();
    void cancel_timer();
    void close();
public:
    Connection();
    Connection(Connection &) = delete;
    Connection & operator = (Connection &) = delete;
    ~Connection();
    void start();
    tcp::socket & socket();
    bool has_closed() const;
    void response();
    void set_conn_id(int64_t id);
    int64_t conn_id();
};
}
}
#endif