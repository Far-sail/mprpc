#include "zookeeperutil.h"
#include "mprpcapplication.h"
#include <iostream>
#include <semaphore.h>

void global_watcher(zhandle_t *zh, int type,
                    int state, const char *path, void *watcherCtx)
{
    if (type == ZOO_SESSION_EVENT) // 回调相关的类型 是和会话相关的消息类型
    {
        if (state == ZOO_CONNECTED_STATE) // zkclient 和 zkserver 连接成功
        {
            sem_t *sem = (sem_t *)zoo_get_context(zh);
            sem_post(sem);
        }
    }
}

ZkClient::ZkClient() : m_zhandle(nullptr)
{
}

ZkClient::~ZkClient()
{
    if (m_zhandle != nullptr)
    {
        zookeeper_close(m_zhandle);
    }
}

// zkClient 启动连接zkserver
void ZkClient::Start()
{
    std::string zk_ip = MprpcApplication::GetInstance()->GetConfig().Load("zookeeperip");
    std::string zk_port = MprpcApplication::GetInstance()->GetConfig().Load("zookeeperport");
    std::string connstr = zk_ip + ":" + zk_port;

    m_zhandle = zookeeper_init(connstr.c_str(), global_watcher, 3000, nullptr, nullptr, 0);
    if (m_zhandle == nullptr)
    {
        std::cout << "zookeeper_init error!" << std::endl;
        exit(EXIT_FAILURE);
    }

    sem_t sem;
    sem_init(&sem, 0, 0);
    zoo_set_context(m_zhandle, &sem);

    sem_wait(&sem);
    std::cout << "zookeeper init success!" << std::endl;
}

// 在zkserver 上根据path 创建一个znode节点
void ZkClient::Create(const char *path, const char *data, int datalen, int state)
{
    // 判断节点是否存在，如果存在，就不创建了
    int flag = zoo_exists(m_zhandle, path, 0, nullptr);
    if (ZNONODE == flag)
    {
        char path_buffer[128] = {0};
        int bufferlen = sizeof(path_buffer);
        flag = zoo_create(m_zhandle, path, data, datalen,
                          &ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);
        if (flag == ZOK)
        {
            std::cout << "znode create success... path:" << path << std::endl;
        }
        else
        {
            std::cout << "flag:" << flag << std::endl;
            std::cout << "znode create error... path:" << path << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

// 根据参数指定的znode节点路径，或者znode节点的值
std::string ZkClient::GetData(const char *path)
{
    char buffer[64];
    int bufferlen = sizeof(buffer);

    int flag = zoo_get(m_zhandle, path, 0, buffer, &bufferlen, nullptr);
    if (flag != ZOK)
    {
        std::cout << "get znode error... path:" << path << std::endl;
        return "";
    }
    return buffer;
}
