#pragma once

#include <iostream>
#include <pthread.h>
#include <signal.h>
#include "Protocol.hpp"
#include "TCPServer.hpp"
#include "Log.hpp"

#define PORT 8081

class HTTPServer
{
private:
    int port;              // 端口号
    TcpServer *tcp_server; // 一个TCPServer的对象
    bool stop;             // 服务器状态，是否是停止

public:
    HTTPServer(int _port = PORT) : port(_port), tcp_server(nullptr), stop(false)
    {
    }
    void InitServer()
    {
        // 忽略掉SIGPIPE信号， 如果不忽略，在写入时server可能崩溃
        signal(SIGPIPE, SIG_IGN);
        tcp_server = TcpServer::GetInstance(port);
    }
    void Loop()
    {
        LOG(INFO, "Loop begin...");
        int listen_sock = tcp_server->Sock();
        while (!stop)
        {
            struct sockaddr_in peer;
            socklen_t len = sizeof(peer);
            int sock = accept(listen_sock, (struct sockaddr *)&peer, &len); // 接受服务器请求
            if (sock < 0)
                continue;
            LOG(INFO, "get a new link");
            int *_sock = new int(sock);
            pthread_t tid;
            pthread_create(&tid, nullptr, Entrance::HandlerRequest, _sock); // 创建线程
            pthread_detach(tid);                                            // 分离线程
        }
    }
    ~HTTPServer()
    {
    }
};