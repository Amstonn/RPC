#include "io_pool.h"
namespace easy_rpc{
namespace rpc_server{
io_pool::io_pool(size_t pool_size):next_io_service_(0){
    if(pool_size == 0) throw std::runtime_error("io_pool size is 0");
    //初始化指定数量的io_services
    for(std::size_t i=0;i<pool_size;i++){
        io_service_ptr io_service(new boost::asio::io_service);
        work_ptr work(new boost::asio::io_service::work(*io_service));
        io_services_.push_back(io_service);
        work_.push_back(work);
    }
}
/*
@brief 运行所有io_service在不同的线程上
*/
void io_pool::run(){
    //注意：线程的执行周期是独立于threads以及指向他的指针的
    std::vector<std::shared_ptr<std::thread>> threads;
    for(std::size_t i=0;i<io_services_.size();i++){
        threads.emplace_back(std::make_shared<std::thread>([](io_service_ptr svr){svr->run();},io_services_[i]));
    }
    for(std::size_t i=0;i<threads.size();i++) threads[i]->join();
}
/*
@brief 停止线程池 停止所有io_services
*/
void io_pool::stop(){
    for(std::size_t i=0;i<io_services_.size();i++){
        io_services_[i]->stop();
    }
}
/*
@brief 从线程池获取一个io_service
*/
boost::asio::io_service &  io_pool::get_io_service(){
    boost::asio::io_service& io_service = *io_services_[next_io_service_];
    ++next_io_service_;
    if(next_io_service_ == io_services_.size()) next_io_service_ = 0;
    return io_service;
}
}
}
