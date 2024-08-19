#include "logger.h"
#include <time.h>
#include <iostream>

// 获取日志的单例
Logger& Logger::GetInstance()
{
    static Logger logger;
    return logger;
}

Logger::Logger()
{
    // 设置写日志线程
    std::thread writeLogTask([&](){
        for (;;)
        {
            // 获取当前的日志，然后取日志信息，写入相应的日志文件中
            time_t now = time(nullptr); // 秒
            tm* nowtm = localtime(&now);
            // 文件命名
            char file_name[128];
            sprintf(file_name, "%d-%d-%d-log.txt", 
                    nowtm->tm_year + 1900, nowtm->tm_mon + 1, nowtm->tm_mday);
            // 打开文件，追加写入
            FILE *pf = fopen(file_name, "a+");
            if (pf == nullptr)
            {
                std::cout << "logger file: " << file_name << "open error!" << std::endl;
                exit(EXIT_FAILURE);
            }
            std::string msg = m_lckQue.Pop();
            int loglevel = -1; // 待处理
            msg.copy((char*)&loglevel, 4, 0);
            char time_buf[128] = {0};
            sprintf(time_buf,
                    "%d:%d:%d => [%s]", 
                    nowtm->tm_hour, 
                    nowtm->tm_min, 
                    nowtm->tm_sec,
                    (loglevel == INFO ? "info" : "error"));
            msg = msg.substr(4);
            msg.insert(0, time_buf);
            
            msg.append("\n");

            fputs(msg.c_str(), pf);
            fclose(pf);
        }
    });
    // 分离线程，守护线程
    writeLogTask.detach();
}
// 设置日志级别
// void Logger::SetLogLevel(LogLevel level)
// {
//     m_loglevel = level;
// }

// 写日志 把日志信息写入lockqueue缓冲区中，外部使用
void Logger::Log(std::string msg)
{
    m_lckQue.Push(msg);
}