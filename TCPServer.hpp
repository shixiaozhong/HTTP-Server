#pragma once

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "Log.hpp"

#define BACKLOG 5

class TcpServer // 将TcpServer设置为单例模式
{
private:
    int port;        // 端口号
    int listen_sock; // 监听套接字
    static TcpServer *svr;

private:
    TcpServer(int _port) : port(_port), listen_sock(-1) {}
    TcpServer(const TcpServer &s) {}

public:
    static TcpServer *GetInstance(int port)
    {
        static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; // 初始化锁， 定义一把静态的锁
        if (svr == nullptr)
        {
            pthread_mutex_lock(&lock); // 加锁
            if (svr == nullptr)
            {
                svr = new TcpServer(port);
                svr->InitServer(); // 进行初始化Server
            }
            pthread_mutex_unlock(&lock); // 解锁
        }
        return svr;
    }
    void InitServer() // 初始化Server
    {
        Socket();
        Bind();
        Listen();
        LOG(INFO, "TcpServer init success...");
    }
    void Socket() // 创建套接字
    {
        listen_sock = socket(AF_INET, SOCK_STREAM, 0); // 创建套接字
        if (listen_sock < 0)
        {
            LOG(FATAL, "create socket error!"); // 监听套接字失败
            exit(1);
        }
        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        LOG(INFO, "create socket success...");
    }
    void Bind() // 绑定
    {
        struct sockaddr_in local;
        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_port = htons(port);
        local.sin_addr.s_addr = INADDR_ANY; // 云服务器不能直接绑定公网IP
        if (bind(listen_sock, (struct sockaddr *)&local, sizeof(local)) < 0)
        {
            LOG(FATAL, "bind socket error!");
            exit(2);
        }
        LOG(INFO, "bind socket success...");
    }
    void Listen() // 监听
    {
        if (listen(listen_sock, BACKLOG) < 0)
        {
            LOG(FATAL, "listen socket error!");
            exit(3);
        }
        LOG(INFO, "listen socket success...");
    }
    int Sock() // 获取监听套接字
    {
        return listen_sock;
    }
    ~TcpServer()
    {
        if (listen_sock >= 0)
            close(listen_sock);
    }
};

TcpServer *TcpServer::svr = nullptr; // 静态成员类外初始化