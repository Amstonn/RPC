#include "rpc-server.h"
using namespace easy_rpc;
using namespace rpc_server;

void hello(Connection* conn,const std::string & str){
    std::cout << "Hello " << str << std::endl;
}

class Person{
private:
    int id;
    std::string name;
    int age;
public:
    Person(int id, const char * name, int age){
        this->id = id;
        this->name = name;
        this->age = age;
    }
    std::string get_person_info(Connection* conn){
        return "ID: " + std::to_string(id) + " Name: " + name + " Age: " + std::to_string(age);
    } 
};
int main(){
    RpcServer server(9870,5);
    Person p(1, "amston", 25);
    server.register_handler<ExecMode::sync>("get_person_info", &Person::get_person_info, &p);
    server.register_handler<ExecMode::sync>("hello", hello);
    server.run();
    getchar();
}