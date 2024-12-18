#ifndef ROUTER
#define ROUTER

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <unordered_map>
#include "codec.h"
#include "constvars.h"
#include "util.h"

namespace easy_rpc{
enum class ExecMode {sync, async}; //枚举值限定在ExecMode枚举类中，类型安全且有作用域限制
namespace rpc_server{
class Connection;
/*
@brief 提供rpc请求的处理回调函数  分辨客户端请求的服务并处理得到结果进行响应
*/
class Router{
private:
    std::unordered_map<std::string,std::function<void(Connection*,const char*,size_t,std::string &,ExecMode&)>> map_invlkers_;//rpc请求的处理回调函数
    std::function<void(const std::string&,std::string &&,Connection*,bool)> callback_to_server_;
    
    Router(){};
    Router(Router &) = delete;
    Router & operator = (Router &) = delete;

    /*
    @brief 非成员函数调用器  
    */
    template<typename F, size_t ... I,typename Arg,typename ... Args>
    static typename std::result_of<F(Connection*,Args...)>::type 
    call_helper(const F& f,const std::index_sequence<I...>&,const std::tuple<Arg,Args...>& tup,Connection *ptr){
        return f(ptr, std::get<I+1>(tup)...);
    }
    /*
    @brief 函数调用 F返回结果为空时有效
    */
    template<typename F,typename Arg,typename... Args>
    static typename std::enable_if<std::is_void<typename std::result_of<F(Connection*,Args...)>::type>::value>::type 
    call(const F& f,Connection* ptr,std::string& result,std::tuple<Arg,Args...>& tp){
        call_helper(f,std::make_index_sequence<sizeof...(Args)>{},tp,ptr);
        result = msgpack_codec::pack_args_str(result_code::OK);
    }

    //F返回结果为不为空时有效
    template<typename F,typename Arg,typename... Args>
    static typename std::enable_if<!std::is_void<typename std::result_of<F(Connection *, Args...)>::type>::value>::type
    call(const F& f,Connection* ptr,std::string& result,std::tuple<Arg,Args...>& tp){
        auto r = call_helper(f,std::make_index_sequence<sizeof...(Args)>{},tp,ptr);
        result = msgpack_codec::pack_args_str(result_code::OK,r);
    }
    /*
    @brief 成员函数调用器
    */
    template<typename F,typename Self,size_t... Indexes, typename Arg,typename... Args>
    static typename std::result_of<F(Self,Connection*,Args...)>::type 
    call_member_helper(const F & f, Self * self,const std::index_sequence<Indexes...>&,const std::tuple<Arg,Args...>& tup,Connection* ptr){
        return (*self.*f)(ptr,std::get<Indexes+1>(tup)...);
    }
    /*
    @brief 成员函数调用 调用函数返回为空的成员函数
    */
    template<typename F,typename Self, typename Arg,typename... Args>
    typename std::enable_if<std::is_void<typename std::result_of<F(Self,Connection*,Args...)>::type>::value>::type
    static call_member(const F& f,Self* self,Connection* ptr,std::string &result,const std::tuple<Arg,Args...>& tp){
        call_member(f,self,typename std::make_index_sequence<sizeof...(Args)>{},tp,ptr);
        result = msgpack_codec::pack_args_str(result_code::OK);
    }
    /*
    @brief 成员函数调用 调用函数返回不为空的成员函数
    */
    template<typename F,typename Self, typename Arg,typename... Args>
    static typename std::enable_if<!std::is_void<typename std::result_of<F(Self,Connection*,Args...)>::type>::value>::type
    call_member(const F& f,Self* self,Connection* ptr,std::string &result,const std::tuple<Arg,Args...>& tp){
        auto r = call_member_helper(f,self,typename std::make_index_sequence<sizeof...(Args)>{}, tp, ptr);
        result = msgpack_codec::pack_args_str(result_code::OK,r);
    }
    /*
    @brief 注册一个非成员函数的服务
    */
    template<ExecMode model,typename Function>
    void register_nonmember_func(std::string const & name,Function f){
        map_invlkers_[name] = {
            //一个新的可调用对象，其函数来自结构体模板中的静态成员函数模板 且固定函数指针  后续调用需要传入其余参数
            std::bind(&invoker<Function>::template apply<model>,std::move(f),std::placeholders::_1,
                std::placeholders::_2,std::placeholders::_3,std::placeholders::_4,std::placeholders::_5)
        };
    }
    /*
    @brief 注册一个成员函数格式的服务
    */
    template<ExecMode model,typename Function, typename Self>
    void register_member_func(const std::string& name,const Function& f, Self* self){
        map_invlkers_[name] = {std::bind(&invoker<Function>::template apply_member<model, Self>,f,self,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4,std::placeholders::_5)};
    }

    /*
    @brief 对参数进行解码并处理函数调用
    */
    template<typename Function, ExecMode mode = ExecMode::sync>
    struct invoker{
        //调用非成员函数 内联函数将在调用时展开为函数代码 避免了函数压栈 减小计算
        template<ExecMode model>
        static inline void apply(const Function& func,Connection* conn, const char* data,size_t size,std::string& result, ExecMode& exe_model){
            using args_tuple = typename function_traits<Function>::args_tuple_2nd;
            exe_model = ExecMode::sync;
            msgpack_codec codec;
            try{
                auto tp = codec.unpack<args_tuple>(data,size);
                call(func,conn,result,tp);
                exe_model = model;
            }catch(std::invalid_argument & e){
                result = codec.pack_args_str(result_code::FAIL,e.what());
            }catch(const std::exception & e){
                result = codec.pack_args_str(result_code::FAIL,e.what());
            }
        }
        //调用成员函数
        template<ExecMode model,typename Self>
        static inline void apply_member(const Function& func,Self *self,Connection* conn,const char *data, size_t size,std::string& result,ExecMode& exe_model){
            using arg_tuple = typename function_traits<Function>::args_tuple_2nd;
            exe_model = ExecMode::sync;
            msgpack_codec codec;
            try{
                auto tp = codec.unpack<arg_tuple>(data,size);
                call_member(func, self, conn, result, tp);
                exe_model = model;
            }catch(std::invalid_argument & e){
                result = codec.pack_args_str(result_code::FAIL,e.what());
            }catch(std::exception &e){
                result = codec.pack_args_str(result_code::FAIL,e.what());
            }
        }
    };
public:
    /*
    @brief 单例模式 获取Router对象
    */
    static Router& get(){
        static Router instance;
        return instance;
    }

    /*
    @brief 注册与调用对象无关的新服务 (注册的函数为非成员函数)
    @param name 服务名称
    @param f 处理函数
    */
    template<ExecMode model, typename Function>
    void register_handler(std::string const & name, Function &f){
        return register_nonmember_func<model> (name, move(f));
    }

    /*
    @brief 注册与调用对象相关的新服务 (注册的函数为类的成员函数)
    @param name 服务名称
    @param f 处理函数
    @param self 调用对象
    */
    template<ExecMode model, typename Function, typename Self>
    void register_handler(std::string const & name, Function &f, Self* self){
        return register_member_func<model>(name,f,self);
    }
    /*
    @brief 移除特定服务
    @param name 需要移除的服务的名称
    */
    void remove_handler(std::string const & name){
        map_invlkers_.erase(name);
    }
    /*
    @brief 设定router当前的回调函数
    @param callback 回调函数
    */
    void set_callback(const std::function<void(const std::string&,std::string &&,Connection*,bool)> &callback){
        callback_to_server_ = callback;
    }
    /*
    @brief 根据传入的请求数据，确定需要调用的服务，调用服务并返回结果
    @param data 传入的请求体数据,往往是被序列化过的数据
    @param size 请求数据大小
    @param conn 连接
    */
    template<typename T>
    void route(const char* data, std::size_t size, T conn){
        std::string result;
        try{
            msgpack_codec codec;
            //反序列化 获取原始请求
            auto p = codec.unpack<std::tuple<std::string>>(data,size);
            //获取p中第一个元素 函数名字
            auto &func_name = std::get<0>(p);
            //查看服务列表中是否有请求的服务
            auto it = map_invlkers_.find(func_name);
            if(it == map_invlkers_.end()){ //服务不存在
                result = codec.pack_args_str(result_code::FAIL,"unknown funciton: " + func_name);
                callback_to_server_(func_name, std::move(result),conn,true);
                return;
            }
            ExecMode model;
            //调用函数并将结果和调用模式写入result和model中
            it->second(conn,data,size,result,model);
            if(model == ExecMode::sync && callback_to_server_){
                //回复数据过长
                if(result.size() >= MAX_BUF_LEN){
                    result = codec.pack_args_str(result_code::FAIL,"The response result is out of range." + func_name);
                    callback_to_server_(func_name,std::move(result),conn,true);
                    return;
                }else{
                    //正常回调
                    callback_to_server_(func_name, std::move(result),conn,false);
                }
            }
        }catch(const std::exception & ex){
            msgpack_codec codec;
            result = codec.pack_args_str(result_code::FAIL,ex.what());
            callback_to_server_("",std::move(result),conn,true);
        }
    }

    ~Router(){
        map_invlkers_.clear();
    }
};
}
}
#endif