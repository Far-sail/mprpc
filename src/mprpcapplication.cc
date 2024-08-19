#include "mprpcapplication.h"
#include <iostream>
#include <unistd.h>

void ShowArgHelp()
{
    std::cout << "format: command -i <configfile>" << std::endl;
}

// 静态变量初始化
MprpcConfig MprpcApplication::m_config; 

// 框架的初始化
void MprpcApplication::Init(int argc, char **argv)
{
    if (argc < 2)
    {
        ShowArgHelp();
        exit(EXIT_FAILURE);
    }

    int c = 0;
    std::string config_file;
    while ((c = getopt(argc, argv, "i:")) != -1)
    {
        switch (c)
        {
        case 'i':
            config_file = optarg;
            break;
        case '?':
            //std::cout << "invalid args!" << std::endl;
            ShowArgHelp();
            exit(EXIT_FAILURE);
        case ':':
            //std::cout << "need <configfile>" << std::endl;
            ShowArgHelp();
            exit(EXIT_FAILURE);
        default:
            break;
        }
    }

    // 加载配置文件 rpcserver_ip rpcserver_port zookeeper_ip zookeeper_port
    m_config.LoadConfigFile(config_file.c_str());
    // std::cout << "rpcserverip:" << m_config.Load("rpcserverip") << std::endl;
    // std::cout << "rpcserverport:" << m_config.Load("rpcserverport") << std::endl;
    // std::cout << "zookeeperip:" << m_config.Load("zookeeperip") << std::endl;
    // std::cout << "zookeeperport:" << m_config.Load("zookeeperport") << std::endl;
}

// 获取框架的单例
MprpcApplication *MprpcApplication::GetInstance()
{
    static MprpcApplication app;
    return &app;
}

MprpcConfig& MprpcApplication::GetConfig() 
{ 
    return MprpcApplication::m_config;
};