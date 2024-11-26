#ifndef ROUTER
#define ROUTER

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include "codec.h"
#include "meta_util.h"

namespace easy_rpc{
enum class ExecMode {sync, async}; //枚举值限定在ExecMode枚举类中，类型安全且有作用域限制
namespace rpc_server{
class Connection;
class Router{
private:


public:
    Router();
    Router(Router &) = delete;
    Router & operator = (Router &) = delete;
    ~Router();
    static Router& get();
    template<ExecMode model, typename Function>
    void register_handler(std::string const & name, Function &f);
    template<ExecMode model, typename Function, typename Self>
    void register_handler(std::string const & name, Function &f, Self* self);
    void remove_handler(std::string const & name);
    void set_callback(const std::function<void(const std::string&,std::string &&,Connection*,bool)> &callback);
    template<typename T>
    void route(const char* data, std::size_t size, T conn);
};
}
}
#endif