#pragma once

#include <iostream>
#include <unistd.h>

class Entrance // 入口类
{
public:
    static void *HandlerRequest(void *_sock)
    {
        int sock = *(int *)_sock;
        delete _sock;
        std::cout << "get a new link...." << sock << std::endl;
        close(sock);
        return nullptr;
    }
};