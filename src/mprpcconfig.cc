#include "mprpcconfig.h"
#include <iostream>

// 加载配置文件，存入hashmap中
void MprpcConfig::LoadConfigFile(const char* config_file)
{
    FILE* pf = fopen(config_file, "r");
    if (nullptr == pf)
    {
        std::cout << config_file << " is not exist." << std::endl;
    }

    while(!feof(pf))
    {
        char buf[512] = {0};
        fgets(buf, 512, pf);

        // 去除字符串首尾空格
        std::string read_buf(buf);
        Trim(read_buf);

        // 判断 #注释 和 空行
        if (read_buf[0] == '#' || read_buf.empty())
        {
            continue;
        }

        int idx = read_buf.find('=');
        if (idx == -1)
        {
            // 这条配置不合法
            continue;
        }
        std::string key = read_buf.substr(0, idx);
        Trim(key);
        int endid = read_buf.find('\n');
        std::string value = read_buf.substr(idx+1, endid - idx - 1);
        Trim(value);
        // 存入 k-v
        m_configMap.insert({key, value});
    }
}

// 根据键，返回值
std::string MprpcConfig::Load(const std::string& key)
{
    auto it = m_configMap.find(key);
    if (it == m_configMap.end())
    {
        return std::string("");
    }
    return it->second;
}

// 删除多余的空格
void MprpcConfig::Trim(std::string& src_buf)
{
    int idx = src_buf.find_first_not_of(' ');
    if (idx != -1)
    {
        // 说明字符串前面可能有空格
        src_buf = src_buf.substr(idx, src_buf.size() - idx);
    }
    idx = src_buf.find_last_not_of(' ');
    if (idx != -1)
    {
        src_buf = src_buf.substr(0, idx + 1);
    }
}