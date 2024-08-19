#pragma once 
#include "zookeeper/zookeeper.h"
#include <string>

// zookeeper 客户端
class ZkClient
{
public:
    ZkClient();
    ~ZkClient();
    // zkClient 启动连接zkserver
    void Start();
    // 在zkserver 上根据path 创建一个znode节点
    void Create(const char* path, const char* data, int datalen, int state = 0);
    // 根据参数指定的znode节点路径，或者znode节点的值
    std::string GetData(const char* path);
private:
    // zk的客户端句柄
    zhandle_t* m_zhandle;
};