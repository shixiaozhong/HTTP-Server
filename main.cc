#include <string>
#include <memory>
#include "HTTPServer.hpp"

static void Usage(std::string proc)
{
    std::cout << "Usage:\n\t" << proc << " port" << std::endl;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        Usage(argv[0]);
        exit(4);
    }
    int port = atoi(argv[1]);
    std::shared_ptr<HTTPServer> http_server(new HTTPServer(port)); // 创建了一个HTTPServer对象
    http_server->InitServer();                                     // 初始化服务器
    http_server->Loop();                                           // 开始循环接收
    return 0;
}