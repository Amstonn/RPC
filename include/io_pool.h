#ifndef IOPOOL
#define IOPOOL
#include <boost/asio.hpp>
#include <vector>
#include <memory>
#include <boost/noncopyable.hpp>
using namespace std;

namespace easy_rpc{
namespace rpc_server{
class io_pool : private boost::noncopyable{
private:
    typedef shared_ptr<boost::asio::io_service> io_service_ptr;  //io_service负责异步操作的事件循环，调度和操作异步循环
    typedef shared_ptr<boost::asio::io_service::work> work_ptr; //work是辅助类，用于确保io_service在有工作可作时不会退出
    vector<io_service_ptr> io_services_; //io_services池
    vector<work_ptr> work_; // 保持io_services运行的work数组
    size_t next_io_service_;//连接中下一个可使用的io_service

public:
    explicit io_pool(size_t pool_size) ;
    void run();
    void stop();
    boost::asio::io_service & get_io_service();
};
}
}
#endif