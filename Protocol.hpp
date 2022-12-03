#pragma once

#include <iostream>
#include <unistd.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include "Util.hpp"
#include "Log.hpp"

#define SEP ": "

// 处理Http的请求
class HttpRequest
{
public:
    // 原始报文数据
    std::string request_line;                // 请求行
    std::vector<std::string> request_header; //请求报头
    std::string blank;                       //空行
    std::string request_body;                // 请求正文

    // 解析完毕之后的结果

    // 存储解析请求行得到的结果
    std::string method;
    std::string uri;
    std::string version;

    // 存储解析报头得到的结果
    std::unordered_map<std::string, std::string> header_kv;

    int content_length; //

public:
    HttpRequest() : content_length(0)
    {
    }
    ~HttpRequest()
    {
    }
};

// 对http的应答
class HttpResponse
{
public:
    std::string status_line;                  // 状态行
    std::vector<std::string> response_header; //请求报头
    std::string blank;                        //空行
    std::string response_body;                // 请求正文
};

// 提供读取请求，分析请求，构建响应
// IO通信
class EndPoint
{
private:
    int sock;
    HttpRequest http_request;
    HttpResponse http_response;

private:
    // 读取请求行
    void RecvHttpRequestLine()
    {
        Util::ReadLine(sock, http_request.request_line); // 读取请求行
        auto &line = http_request.request_line;
        line.resize(line.size() - 1); // 去掉 \n
        LOG(INFO, line);
    }

    // 读取请求报头
    void RecvHttpRequestHeader()
    {
        std::string line;
        while (true)
        {
            line.clear();
            Util::ReadLine(sock, line);
            if (line == "\n")
            {
                http_request.blank = line; // 将读到的空行放到blank中
                break;                     // 读到空行代表报头部分结束
            }
            line.resize(line.size() - 1);
            http_request.request_header.push_back(line); // 将读到的整行push_back到requese_header中
            LOG(INFO, line);
        }
    }

    // 解析请求行， 拿到方法，URI，HTTP版本
    void ParseHttpRequestLine()
    {
        auto &line = http_request.request_line;
        std::stringstream ss(line);
        ss >> http_request.method >> http_request.uri >> http_request.version; // 默认按照空格来分割
    }

    // 解析报头
    void ParseHttpRequestHeader()
    {
        std::string key, value;

        for (auto &iter : http_request.request_header)
        {
            if (Util::CutString(iter, key, value, SEP)) // 分隔符定义为宏，一般为 ": "
            {
                http_request.header_kv[key] = value; // 插入到unordered_map中
            }
        }
    }

    // 判断是否需要读取正文部分,正文部分不一定存在
    bool IsNeedRecvHttpRequestBody()
    {
        auto method = http_request.method;
        if (method == "POST")
        {
            auto &header_kv = http_request.header_kv;
            auto iter = header_kv.find("content_length");
            if (iter != header_kv.end())
            {
                http_request.content_length = atoi(iter->second.c_str()); // 如果存在的话就储存在content_length中
                return true;
            }
        }
        return false;
    }

    //读取正文部分
    void RecvHttpRequestBody()
    {
        if (IsNeedRecvHttpRequestBody())
        {
            int content_length = http_request.content_length;
            auto &body = http_request.request_body;
            char ch = '0'; // 初始化为 0
            while (content_length)
            {
                if (recv(sock, &ch, 1, 0) > 0)
                {
                    body.push_back(ch);
                    content_length--;
                }
                else
                    break;
            }
        }
    }

public:
    EndPoint(int _sock) : sock(_sock)
    {
    }

    // 读取请求
    void RecvHttpRequest()
    {
        RecvHttpRequestLine();   // 读取请求行
        RecvHttpRequestHeader(); // 读取请求报头

        // 解析请求
        ParseHttpRequestLine();   // 解析请求行
        ParseHttpRequestHeader(); // 解析报头
        RecvHttpRequestBody();    // 读取正文部分
    }

    // 构建响应
    void BuildHttpResponse()
    {
    }
    // 发送响应
    void SendHttpResponse()
    {
    }
    ~EndPoint()
    {
        close(sock); // 析构的时候关闭
    }
};

// 入口类
class Entrance
{
public:
    static void *HandlerRequest(void *_sock)
    {
        LOG(INFO, "handle request begin...");
        int sock = *(int *)_sock;
        delete (int *)_sock;
        // // std::cout << "get a new link...." << sock << std::endl;
        // std::string line;           // 存储读取到的行
        // Util::ReadLine(sock, line); // 按行来读取
        // std::cout << line << std::endl;
// 调试部分
#ifdef DEBUG
        char buffer[4096];
        recv(sock, buffer, sizeof(buffer), 0); // 读数据
        std::cout << "----------------------begin------------------" << std::endl;
        std::cout << buffer << std::endl;
        std::cout << "----------------------end--------------------" << std::endl;
#else
        EndPoint *ep = new EndPoint(sock);
        ep->RecvHttpRequest();   // 读取请求
        ep->BuildHttpResponse(); // 构建响应
        ep->SendHttpResponse();  // 发送响应
        delete ep;
#endif
        LOG(INFO, "handle request end...");
        return nullptr;
    }
};