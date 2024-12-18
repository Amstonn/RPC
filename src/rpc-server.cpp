#include "rpc-server.h"
namespace easy_rpc{
namespace rpc_server{
/*
@brief 构造函数
@param port 服务端口号
@param size io服务池大小
@param timeout_seds 请求超时时间
@param check_seds 检查时间间隔
*/
RpcServer::RpcServer(short port, size_t size, size_t timeout_seds, size_t check_seds)
    :io_pool_(size),acceptor_(io_pool_.get_io_service(), tcp::endpoint(tcp::v4(),port)),
    timeout_second_(timeout_seds),__check_seconds_(check_seds)
{
    //设置路由的回调函数
    Router::get().set_callback(std::bind(&RpcServer::callback, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4));
    //开始接受连接
    do_accept();
    //创建连接检查线程
    check_thread_ = std::make_shared<std::thread>([this]{clean();});
}

RpcServer::~RpcServer(){
    stop_check_ = true;
    check_thread_->join(); //等待检查线程结束
    io_pool_.stop();//关闭连接池
    thd_->join();
}
/*
@brief 接受客户端连接 该接受函数在子线程中往复执行，实质上是在遍历服务池中的所有io连接，并将有连接请求的连接加入本地连接中
*/
void RpcServer::do_accept(){
    conn_.reset(new Connection(io_pool_.get_io_service(), timeout_second_));
    //异步接受连接
    acceptor_.async_accept(conn_->socket(), [this](boost::system::error_code ec){
        if(!ec){
            conn_->start();//连接建立 
            std::unique_lock<std::mutex> lock(mtx_);
            conn_->set_conn_id(conn_id);
            connections_.emplace(conn_id ++ , conn_);
        }
        do_accept();
    });
}
/*
@brief check线程执行的函数 清理超时或者不再使用的连接
*/
void RpcServer::clean(){
    while(!stop_check_){
        //检查时间阻塞 避免频繁检查
        std::this_thread::sleep_for(std::chrono::seconds(__check_seconds_));
        std::unique_lock<std::mutex> lock(mtx_);
        for(auto it= connections_.cbegin();it != connections_.cend();){
            if(it->second->has_closed()){
                //注意：erase后返回的迭代器是删除位置的后一个
                it = connections_.erase(it);
            }else it ++;
        }
    }
}
/*
@brief Router回调函数 将函数调用结果返回客户端
*/
void RpcServer::callback(const std::string &topic, std::string&& result,Connection * conn, bool has_error){
    response(conn->get_conn_id(), std::move(result));
}
/*
@brief 运行RPC服务端
*/
void RpcServer::run(){
    //启动服务线程，服务线程实质是创建IO服务池 该线程会等待所有服务线程结束 
    thd_ = std::make_shared<std::thread>([this]{io_pool_.run();});
}
/*
@brief 向指定连接回复数据
*/
void RpcServer::response(int64_t conn_id, std::string && result){
    std::unique_lock<mutex> lock(mtx_);
    auto it = connections_.find(conn_id);
    if(it != connections_.end()) {
        it->second->response(std::move(result));
    }
}
}
}