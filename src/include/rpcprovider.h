#pragma once
#include "google/protobuf/service.h"
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <string>
#include <functional>
#include <google/protobuf/descriptor.h>
#include <unordered_map>

// 框架提供的专门发布rpc服务的网络对象类
class RpcProvider
{
public:
    // 提供给外部使用，可以发布 rpc 方法的函数接口
    void NotifyService(::google::protobuf::Service* service);
    // 启动 rpc 服务节点，开始提供 rpc 远程网络调用服务
    void Run();
private:
    muduo::net::EventLoop m_eventLoop;
    
    struct ServiceInfo
    {
        ::google::protobuf::Service* m_service;
        std::unordered_map<std::string,const google::protobuf::MethodDescriptor*> m_methodMap;
    };
    std::unordered_map<std::string, ServiceInfo> m_serviceMap;
    // 连接事件
    void OnConnection(const muduo::net::TcpConnectionPtr&);
    // 消息事件
    void OnMessage(const muduo::net::TcpConnectionPtr&,
                            muduo::net::Buffer*,
                            muduo::Timestamp);
    // Closure 的回调操作，用于序列化Rpc的响应和网络发送
    void SendRpcResponse(const muduo::net::TcpConnectionPtr&, google::protobuf::Message*);
};