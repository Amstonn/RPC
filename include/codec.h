#ifndef CODEC
#define CODEC
#include <msgpack.hpp>

namespace easy_rpc{
namespace rpc_server{
    using buffer_type = msgpack::sbuffer;
    /*
    @brief 一个序列化与反序列化工具
    基于msgpack实现，msgpack使用二进制变长编码实现序列化，有助于提升传输效率
    */
    struct msgpack_codec{
        const static size_t init_size = 2*1024;

        //将数据打包到buffer中 可变参数函数，将多个参数打包成一个 MsgPack 的二进制格式
        template<typename... Args>
        static buffer_type pack_args(Args&&... args){
            buffer_type buffer(init_size);
            msgpack::pack(buffer,std::forward_as_tuple(std::forward<Args>(args)...));
            return buffer;
        };

        //处理包含枚举的参数，将参数打包为 MsgPack 字符串格式
        template<typename Arg,typename... Args,typename=typename std::enable_if<std::is_enum<Arg>::value>::type>
        static std::string pack_args_str(Arg arg, Args&&... args){
            buffer_type buffer(init_size);
            //forward实现完美转发  保证值类型避免不必要的拷贝
            msgpack::pack(buffer,std::forward_as_tuple((int)arg,std::forward<Args>(args)...));
            return std::string(buffer.data(),buffer.size());
        }

        //用于单一对象的序列化
        template<typename T>
        buffer_type pack(T&& t)const{
            buffer_type buffer;
            msgpack::pack(buffer, std::forward<T>(t));
            return buffer;
        }

        //反序列化
        template<typename T>
        T unpack(const char * data, size_t length){
            try{
                msgpack::unpack(msg_, data, length);
                return msg_.get().as<T>();
            }catch(...){
                throw std::invalid_argument("unpack failed: Args not match!");
            }
        }
        private:
            msgpack::unpacked msg_; //反序列化的内容

    };
}
}
#endif