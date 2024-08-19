#include "mprpcchannel.h"
#include <string>
#include <rpcheader.pb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include "mprpcapplication.h"
#include <arpa/inet.h>
#include <unistd.h>
#include "zookeeperutil.h"

/*
header_size + service_name + method_name + args_size + args
*/
// 所有通过 stub 代理对象调用的 rpc 方法，都会调用这个方法，统一做rpc方法数据的序列化和网络发送
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                              google::protobuf::RpcController *controller,
                              const google::protobuf::Message *request,
                              google::protobuf::Message *response,
                              google::protobuf::Closure *done)
{
    // 获取参数的序列化字符串长度 args_size
    uint32_t args_size = 0;
    std::string args_str;
    if (request->SerializeToString(&args_str))
    {
        args_size = args_str.size();
    }
    else
    {
        controller->SetFailed("serialize request error!");
        return;
    }

    // 定义 rpc消息的头部
    const google::protobuf::ServiceDescriptor* sd = method->service();
    std::string service_name = sd->name(); 
    std::string method_name = method->name();
    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);
    // 序列化头部
    std::string rpc_header_str;
    uint32_t header_size = 0;
    if (rpcHeader.SerializeToString(&rpc_header_str))
    {
        header_size = rpc_header_str.size();  
    }
    else
    {
        // 头部 序列化失败
        controller->SetFailed("serialize rpc header error!");
        return;
    }

    // 组织待发送的rpc请求字符串
    std::string send_rpc_str;
    send_rpc_str.insert(0, std::string((char*)&header_size, 4));
    send_rpc_str += rpc_header_str;
    send_rpc_str += args_str;
    
    // 打印调试信息
    std::cout << "===============" << std::endl;
    std::cout << "header_size:" << header_size << std::endl; 
    std::cout << "rpc_header_str:" << rpc_header_str << std::endl; 
    std::cout << "service_name:" << service_name << std::endl; 
    std::cout << "method_name:" << method_name << std::endl; 
    std::cout << "args_size:" << args_size << std::endl; 
    std::cout << "args_str:" << args_str << std::endl; 
    std::cout << "===============" << std::endl;

    // tcp编程，socket 向服务器发送这个字符串
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == client_fd)
    {
        char errtxt[512] = {0};
        sprintf(errtxt, "socket create failed! errno: %d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // 读取配置文件
    // std::string ip = MprpcApplication::GetInstance()->GetConfig().Load("rpcserverip");
    // uint16_t port = atoi(MprpcApplication::GetInstance()->GetConfig().Load("rpcserverport").c_str());
    ZkClient zkCli;
    zkCli.Start();
    std::string method_path = "/" + service_name + "/" + method_name;
    std::string data = zkCli.GetData(method_path.c_str());
    if (data == "")
    {
        controller->SetFailed(method_path + " is not exist!");
        return;
    }
    int idx = data.find(":");
    if (idx == -1)
    {
        controller->SetFailed(method_path + " address is invalid!");
        return;
    }
    std::string ip = data.substr(0, idx);
    uint16_t port = std::atoi(data.substr(idx+1, data.size() - idx).c_str());


    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    // 连接
    if (-1 == connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)))
    {
        char errtxt[512] = {0};
        sprintf(errtxt, "Connect failed! errno: %d", errno);
        controller->SetFailed(errtxt);
        close(client_fd);
        return;
    }

    // 发送 rpc 请求
    if (-1 == send(client_fd, send_rpc_str.c_str(), send_rpc_str.size(), 0))
    {
        char errtxt[512] = {0};
        sprintf(errtxt, "Send failed! errno: %d", errno);
        controller->SetFailed(errtxt);
        close(client_fd);
        return;
    }

    // 接受 rpc 响应
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if (-1 == (recv_size = recv(client_fd, recv_buf, 1024, 0)))
    {
        char errtxt[512] = {0};
        sprintf(errtxt, "Recv failed! errno: %d", errno);
        controller->SetFailed(errtxt);
        close(client_fd);
        return;
    }
    
    // 反序列化 rpc 调用的响应
    // std::string response_str(recv_buf, 0, recv_size); // bug, recv_buf 中遇到 \0 后面的数据就存不下来了，导致序列化失败
    // if (!response->ParseFromString(response_str))
    if (!response->ParseFromArray(recv_buf, recv_size))
    {
        char errtxt[1088] = {0};
        sprintf(errtxt, "Parse failed! response_str: %s", recv_buf); // 拼接字符数组
        controller->SetFailed(errtxt);
        close(client_fd);
        return;
    }

    close(client_fd);

}