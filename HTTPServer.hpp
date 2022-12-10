#pragma once

#include <iostream>
#include <pthread.h>
#include <signal.h>
#include "Protocol.hpp"
#include "TCPServer.hpp"
#include "Log.hpp"
#include "Task.hpp"
#include "ThreadPool.hpp"

#define PORT 8081

class HTTPServer
{
private:
    int port;  // 端口号
    bool stop; // 服务器状态，是否是停止

public:
    HTTPServer(int _port = PORT) : port(_port), stop(false)
    {
    }
    void InitServer()
    {
        // 忽略掉SIGPIPE信号， 如果不忽略，在写入时server可能崩溃
        signal(SIGPIPE, SIG_IGN);
    }
    void Loop()
    {
        TcpServer *tcp_server = TcpServer::GetInstance(port);
        LOG(INFO, "Loop begin...");
        while (!stop)
        {
            struct sockaddr_in peer;
            socklen_t len = sizeof(peer);
            int sock = accept(tcp_server->Sock(), (struct sockaddr *)&peer, &len); // 接受服务器请求
            if (sock < 0)
                continue;
            LOG(INFO, "get a new link");
            Task task(sock);
            ThreadPool::GetInstance()->PushTask(task);
        }
    }
    ~HTTPServer()
    {
    }
};