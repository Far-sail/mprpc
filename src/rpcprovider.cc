#include "rpcprovider.h"
#include "mprpcapplication.h"
#include "rpcheader.pb.h"
#include "zookeeperutil.h"

// 框架提供给外部使用的，可以发布 rpc 方法的函数接口
void RpcProvider::NotifyService(::google::protobuf::Service* service)
{
    ServiceInfo serviceInfo;
    // 获取服务对象的描述信息
    const ::google::protobuf::ServiceDescriptor* pserverDesc  = service->GetDescriptor();
    // 获取服务的名字
    std::string serviceName = pserverDesc->name();
    // 获取服务对象方法的数量
    int methodCnt = pserverDesc->method_count();

    std::cout << "serviceName:" << serviceName << std::endl;
    
    for (int i = 0; i < methodCnt; ++i)
    {
        // 获取方法
        const google::protobuf::MethodDescriptor* pMethodDesc = pserverDesc->method(i);
        // 获取方法名
        std::string methodName = pMethodDesc->name();
        
        std::cout << "methodName:" << methodName << std::endl;

        serviceInfo.m_methodMap.insert({methodName, pMethodDesc});
    }
    serviceInfo.m_service = service;
    m_serviceMap.insert({serviceName, serviceInfo});
}

void RpcProvider::Run()
{
    // rpc服务器的 ip 和 port
    std::string ip = MprpcApplication::GetInstance()->GetConfig().Load("rpcserverip");
    uint16_t port = std::atoi(MprpcApplication::GetInstance()->GetConfig().Load("rpcserverport").c_str());
    muduo::net::InetAddress address(ip, port);

    // 创建 TcpServer 对象
    muduo::net::TcpServer server(&m_eventLoop, address, "RpcProvider");
    // 绑定回调方法
    server.setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&RpcProvider::OnMessage, this,
             std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    
    // 设置线程数
    server.setThreadNum(4);

    // zookeeper 服务注册
    ZkClient zkCli;
    // 连接 zkserver
    zkCli.Start(); 
    // 创建 znode， 把要发布的服务，全部注册到zk上面，让rpc client 可以从zk上发现服务
    // session timeout 30s zkclient 网络IO线程 1/3 * timeout 时间发送ping消息，心跳消息
    for (auto& sp : m_serviceMap) 
    {
        // /service_name /UserServiceRpc
        std::string service_path = "/" + sp.first;
        zkCli.Create(service_path.c_str(), nullptr, 0); //创建无数据的永久父节点，如果存在，不创建
        for (auto &mp : sp.second.m_methodMap)
        {
            std::string method_path = service_path + "/" + mp.first;
            char method_path_data[128] = {0};
            sprintf(method_path_data, "%s:%d", ip.c_str(), port);
            zkCli.Create(method_path.c_str(), method_path_data, 128, ZOO_EPHEMERAL);// 创建有数据的临时性子节点，如果存在，不创建
        }
    }

    // rpc 服务端准备启动，打印信息
    std::cout << "RpcProvider start service at ip: " << ip << " port:" << port << std::endl;
    // 服务启动
    server.start();
    // 事件监听 -- 阻塞状态
    m_eventLoop.loop();
}

// 连接事件
void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
    // 断开连接
    if (!conn->connected())
    {
        conn->shutdown();
    }
}

// 消息事件
/*
框架内，RpcProvider 和 RpcConsumer 商量好通信用的 protobuf 数据类型
    service_name method_name args
定义 proto 文件，进行数据头的序列化和反序列化

header_size(4字节) + head_str + args_str Tcp消息的粘包问题
*/
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr& conn,
                        muduo::net::Buffer* buffer,
                        muduo::Timestamp timestamp)
{
    // 接受的数据字符流
    std::string recv_buf = buffer->retrieveAllAsString();
    // 解析
    uint32_t header_size = 0;
    recv_buf.copy((char*)&header_size, 4, 0);

    // 反序列化头部信息
    std::string rpc_header_str = recv_buf.substr(4, header_size);
    mprpc::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;
    if (rpcHeader.ParseFromString(rpc_header_str))
    {
        // 反序列化成功
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }
    else
    {
        // 反序列化失败
        std::cout << "rpc_head_str:" << rpc_header_str << ", parse error!" << std::endl;
        return;
    }

    // 获取rpc方法参数的字符流数据
    std::string args_str = recv_buf.substr(4 + header_size, args_size);

    // 打印调试信息
    std::cout << "===============" << std::endl;
    std::cout << "header_size:" << header_size << std::endl; 
    std::cout << "rpc_header_str:" << rpc_header_str << std::endl; 
    std::cout << "service_name:" << service_name << std::endl; 
    std::cout << "method_name:" << method_name << std::endl; 
    std::cout << "args_size:" << args_size << std::endl; 
    std::cout << "args_str:" << args_str << std::endl; 
    std::cout << "===============" << std::endl;

    // 获取 service 对象和 method 对象
    auto it = m_serviceMap.find(service_name);
    if (it == m_serviceMap.end())
    {
        std::cout << service_name << " is not exist." << std::endl;
        return;
    }

    auto mit = it->second.m_methodMap.find(method_name);
    if (mit == it->second.m_methodMap.end())
    {
        std::cout << method_name << " is not exist." << std::endl;
        return;
    }
    
    google::protobuf::Service* service = it->second.m_service; // 获取 service 对象
    const google::protobuf::MethodDescriptor* method = mit->second;// 获取 method 对象
    
    // 生成 Rpc 方法 的 Request 、 Response
    google::protobuf::Message* request = service->GetRequestPrototype(method).New();
    if (!request->ParseFromString(args_str))
    {
        std::cout << "request parse error, content:" << args_str << std::endl;
        return;
    }
    google::protobuf::Message* response = service->GetResponsePrototype(method).New();

    // 给method方法绑定一个Closure的回调函数
    google::protobuf::Closure* done = 
        google::protobuf::NewCallback<RpcProvider,
                                      const muduo::net::TcpConnectionPtr&,
                                      google::protobuf::Message*>
                                      (this, 
                                       &RpcProvider::SendRpcResponse,
                                       conn, response);

    // 在框架上，根据远端rpc请求，调用当前rpc节点上发布的方法
    service->CallMethod(method, nullptr, request, response, done);

}

// Closure 的回调操作，用于序列化Rpc的响应和网络发送
void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message* response)
{
    std::string response_str;
    if (response->SerializeToString(&response_str)) // Response 序列化
    {
        conn->send(response_str);
    }
    else
    {
        std::cout << "serialize response_str error!" << std::endl;
    }
    conn->shutdown();// 模拟 http 的短链接服务，由 rpcprovider 主动断开连接
}