#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include "Util.hpp"
#include "Log.hpp"

#define SEP ": "                // 分隔符
#define WEB_ROOT "wwwroot"      // web根目录
#define HOME_PAGE "index.html"  // 首页
#define HTTP_VERSION "HTTP/1.0" // http版本
#define LINE_END "\r\n"         // 行分隔符
#define PAGE_404 "404.html"     // 404页面

// 响应状态码
#define OK 200
#define BAD_REQUEST 400
#define NOT_FOUND 404
#define SERVER_ERROR 500

// 处理状态码的描述
static std::string Code2Desc(int code)
{
    std::string desc;
    switch (code)
    {
    case 200:
        desc = "OK";
        break;
    case 404:
        desc = "Not Found";
        break;
    default:
        break;
    }
    return desc;
}

// 处理请求文件的类型
static std::string Suffix2Desc(const std::string &suffix)
{
    // 可以自行添加对应文件的content_type对应关系
    static std::unordered_map<std::string, std::string> suffix2desc = {
        {".html", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".jpg", "application/x-jpg"},
    };
    auto iter = suffix2desc.find(suffix);
    if (iter != suffix2desc.end())
        return iter->second;
    return "text/html"; // 没有找到统一按照html来处理
}

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
    std::string method;  // 方法
    std::string uri;     // uri = path?args,按照?来分隔的
    std::string version; // 协议版本

    // 存储解析报头得到的结果
    std::unordered_map<std::string, std::string> header_kv;

    int content_length;       // 正文长度
    std::string path;         // 请求资源的路径
    std::string suffix;       // 请求资源的后缀，表示文件类型
    std::string query_string; // 参数

    bool cgi;
    int size; // 请求文件的大小
public:
    HttpRequest() : content_length(0), cgi(false)
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
    std::vector<std::string> response_header; // 响应报头
    std::string blank;                        // 空行
    std::string response_body;                // 响应正文
    int status_code;                          // 状态码
    int fd;                                   // 请求文件的文件描述符
public:
    // 状态码默认设置为200
    HttpResponse() : blank(LINE_END), status_code(OK), fd(-1)
    {
    }
    ~HttpResponse()
    {
    }
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
        // 将方法全部转换为大写
        auto &method = http_request.method;
        transform(method.begin(), method.end(), method.begin(), ::toupper);
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
            auto iter = header_kv.find("Content-Length");
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

    // 使用cgi来处理数据
    int ProcessCgi()
    {
        int code = OK;

        // 父进程数据
        auto &method = http_request.method;
        auto &query_string = http_request.query_string;   // uri中提取出来的参数，来自GET方法
        auto &body_text = http_request.request_body;      // 请求正文中数据， 来自POST方法
        int content_length = http_request.content_length; // 发送给子进程的数据长度

        std::string query_string_env;                     // query_string的环境变量
        std::string method_env;                           // method的环境变量
        std::string content_length_env;                   // content_length的环境变量
        auto &bin = http_request.path;                    // 要让子进程执行的目标程序
        auto &respose_body = http_response.response_body; // 响应正文

        int input[2];  // 父进程从input读数据
        int output[2]; // 父进程从output写数据

        if (pipe(input) < 0)
        {
            LOG(ERROR, "pipe input error!");
            code = SERVER_ERROR;
            return code;
        }
        if (pipe(output) < 0)
        {
            LOG(ERROR, "pipe input error!");
            code = SERVER_ERROR;
            return code;
        }

        // 新线程走到这里，但是只有httpserver一个进程，需要创建子进程来执行cgi程序
        pid_t pid = fork();
        if (pid == 0)
        {
            // child，进行exec程序替换
            close(input[0]);  // 关闭input读端
            close(output[1]); // 关闭output写端

            // 将请求的方法也传递给子进程，子进程通过环境变量来确定是哪种方法传递的数据，从而确定是从管道还是环境变量中读写
            method_env = "METHOD=";
            method_env += method;
            putenv((char *)method_env.c_str()); // putenv函数将新添加的环境变量导入到进程的上下文当中

            // GET方法传递的数据通过环境变量来传递，环境变量具有全局属性,GET传递的参数一般比较少，使用环境变量效率较高
            if (method == "GET")
            {
                query_string_env = "QUERY_STRING=";
                query_string_env += query_string;
                putenv((char *)query_string_env.c_str());
                LOG(INFO, "Get Method Add Query_String Env");
            }

            // 将POST方法发送数据的长度通过环境变量传递给子进程
            else if (method == "POST")
            {
                content_length_env = "CONTENT_LENGTH=";
                content_length_env += std::to_string(content_length);
                putenv((char *)content_length_env.c_str());
                LOG(INFO, "Post Method Add Content_Length Env");
            }
            else
            {
                // Do Nothing
            }

            // 替换成功以后，目标子进程如何得知，对应的读写文件的描述符是多少?
            // 做一种约定，替换后的进程，读取管道等价于读取标准输入，写入管道，等价于写入到标准输出
            dup2(output[0], 0); // 将标准输入重定向到output[0]
            dup2(input[1], 1);  // 将标准输出重定向到input[1]

            execl(bin.c_str(), bin.c_str(), nullptr); //执行目标程序
            exit(1);                                  // 失败了直接子进程退出
        }
        else if (pid > 0)
        {
            close(input[1]);  // 关闭input写端
            close(output[0]); // 关闭output读端

            // 如果方法为POST
            if (method == "POST")
            {
                const char *start = body_text.c_str();
                int total = 0;
                int size = 0;
                // 因为管道容量可能一次无法写入这么多的数据，所以可能需要多次写入
                while ((total < content_length) && (size = write(output[1], start + size, body_text.size() - total)) > 0)
                    total += size;
            }

            char ch = 0;
            // 子进程处理完数据后需要返回给父进程
            while (read(input[0], &ch, 1) > 0)
            {
                // push到响应正文中
                respose_body.push_back(ch);
            }
            // 处理子进程是否正常退出
            int status = 0;
            pid_t ret = waitpid(pid, &status, 0); // 阻塞式的等待
            if (ret == pid)
            {
                // 判断子进程是不是正常退出
                if (WIFEXITED(status))
                {
                    // 退出码是不是0
                    if (WEXITSTATUS(status) == 0)
                    {
                        code = OK;
                    }
                    else
                    {
                        code = BAD_REQUEST;
                    }
                }
                else
                {
                    code = SERVER_ERROR;
                }
            }
            // 子进程完成以后关闭文件描述符
            close(input[0]);
            close(output[1]);
        }
        else
        {
            // 创建子进程失败
            LOG(ERROR, "fork error!");
            return 404;
        }
        return code;
    }

    // 以非cgi的方式来处理请求
    int ProcessNonCgi()
    {
        // 打开待发送的文件
        http_response.fd = open(http_request.path.c_str(), O_RDONLY);
        if (http_response.fd >= 0)
            return OK;
        return NOT_FOUND;
    }

    // 构建正确的响应
    void BuildOkResponse()
    {
        std::string line = "Content-Type: ";
        line += Suffix2Desc(http_request.suffix);
        line += LINE_END;
        http_response.response_header.push_back(line);
        line = "Content-Length: ";
        if (http_request.cgi == true)
            line += std::to_string(http_response.response_body.size());
        else
            line += std::to_string(http_request.size); // GET

        line += LINE_END;
        http_response.response_header.push_back(line);
    }

    // 处理返回的错误页面
    void HandlerError(std::string page)
    {
        // 只要出错了，就直接将cgi设为false，当作nocgi来处理
        http_request.cgi = false;
        // 构建响应报头
        http_response.fd = open(page.c_str(), O_RDONLY);
        if (http_response.fd > 0)
        {
            struct stat st;
            stat(page.c_str(), &st);
            http_request.size = st.st_size; // 更新size

            std::string line = "Content-Type: text/html";
            line += LINE_END;
            http_response.response_header.push_back(line);
            line = "Content-Length: ";
            line += std::to_string(st.st_size);
            line += LINE_END;
            http_response.response_header.push_back(line);
        }
    }

    // 错误处理程序,构建出错的响应报文
    void BuildHttpResponseHelper()
    {
        auto &code = http_response.status_code; // 拿到状态码

        // 构建响应状态行
        auto &status_line = http_response.status_line;
        status_line = HTTP_VERSION;
        status_line += " ";
        status_line += std::to_string(code);
        status_line += " ";
        status_line += Code2Desc(code);
        status_line += LINE_END;
        // 返回对应的处理页面
        std::string path = WEB_ROOT;
        path += "/";
        switch (code)
        {
        case OK:
            BuildOkResponse();
            break;
        case NOT_FOUND:
            path += PAGE_404; // 拼接上wwwroot目录
            HandlerError(path);
            break;
        case BAD_REQUEST:
            path += PAGE_404; // 拼接上wwwroot目录
            HandlerError(path);
            break;
        case SERVER_ERROR:
            path += PAGE_404; // 拼接上wwwroot目录
            HandlerError(path);
            break;
        default:
            break;
        }
    }

public:
    EndPoint(int _sock) : sock(_sock)
    {
    }

    // 读取和解析请求
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
        std::string path;                       // 临时存储路径
        auto &code = http_response.status_code; // 存储响应状态码
        std::size_t found = 0;                  // 用于截取请求文件后缀
        // 非法请求，只支持GET和POST方法
        if (http_request.method != "GET" && http_request.method != "POST")
        {
            LOG(WARRING, "method is not right");
            code = BAD_REQUEST; // 状态码设置
            goto END;           // 跳到END标签
        }
        // 只有GET方法才是可以带参数的，设置GET方法请求的资源路径
        if (http_request.method == "GET")
        {
            size_t pos = http_request.uri.find('?');
            if (pos != std::string::npos)
            {
                // 调用CutSTring按照?进行切割字符串
                Util::CutString(http_request.uri, http_request.path, http_request.query_string, "?");
                // GET方法携带参数，需要使用cgi处理数据
                http_request.cgi = true;
            }
            else
            {
                http_request.path = http_request.uri;
            }
        }
        else if (http_request.method == "POST")
        {
            // 设置post请求的资源路径
            http_request.path = http_request.uri;
            // 使用cgi来处理数据
            http_request.cgi = true;
        }
        else
        {
            // TODO，扩展方法，目前只处理GET和POST方法
        }
        // 给path拼上web根目录
        path = http_request.path;
        http_request.path = WEB_ROOT;
        http_request.path += path;
        // 特殊处理请求根目录的情况,请求根目录的情况下提供首页的页面
        if (http_request.path[http_request.path.size() - 1] == '/')
        {
            http_request.path += HOME_PAGE;
        }
        // std::cout << "debug: path=: " << http_request.path << std::endl;

        // 确认访问的资源是否存在
        struct stat st;
        // stat获取文件的元数据
        if (stat(http_request.path.c_str(), &st) == 0)
        {
            // 资源存在
            // 请求的资源是目录，不被允许，直接跳转到首页
            if (S_ISDIR(st.st_mode))
            {
                http_request.path += "/";
                http_request.path += HOME_PAGE;
                stat(http_request.path.c_str(), &st); // 如果是目录，就更新一下文件信息
            }
            // 请求的资源是可执行文件
            if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
            {
                // 特殊处理,使用cgi来处理
                http_request.cgi = true;
            }
            http_request.size = st.st_size; // 设置请求文件的大小
        }
        else
        {
            // 资源不存在
            LOG(WARRING, http_request.path + " Not Found!");
            code = NOT_FOUND;
            goto END;
        }

        // 提取文件后缀
        found = http_request.path.rfind("."); // 从后往前找，可能存在多个后缀
        if (found == std::string::npos)
        {
            // 没找到
            http_request.suffix = ".html"; // 默认设置为html
        }
        else
        {
            http_request.suffix = http_request.path.substr(found); // 提取后缀
        }
        // 是否需要使用cgi
        if (http_request.cgi)
        {
            code = ProcessCgi(); // 使用cgi的方式来处理请求，执行目标程序，拿到结果：http_response.response_body
        }
        else
        {
            // 1. 目标网页一定是存在的
            // 2. 返回需要构建Http响应
            code = ProcessNonCgi(); // 使用非cgi的方式来处理请求，简单的网页返回，返回静态网页，只需要打开就行
            // 可能存在打开文件出错等问题，从而修改状态码code
        }
    // END标签，处理错误信息
    END:
        BuildHttpResponseHelper(); // 响应状态行填充了，响应报头也填充了，正文也填充了
    }

    // 发送响应
    void SendHttpResponse()
    {
        auto &status_line = http_response.status_line;
        send(sock, status_line.c_str(), status_line.size(), 0); // 发送状态行
        for (auto &iter : http_response.response_header)        // 发送响应报头
            send(sock, iter.c_str(), iter.size(), 0);
        send(sock, http_response.blank.c_str(), http_response.blank.size(), 0); // 发送空行

        // 发送正文部分
        // 通过cgi处理后
        if (http_request.cgi == true)
        {
            auto &response_body = http_response.response_body;
            size_t size = 0;
            size_t total = 0;
            const char *start = response_body.c_str();
            std::cout << "response_body: " << response_body << std::endl;
            while ((total < response_body.size()) && (size = send(sock, start + total, response_body.size() - total, 0)) > 0)
                total += size;
        }
        // 非cgi处理后
        else
        {
            sendfile(sock, http_response.fd, nullptr, http_request.size); // 发送正文部分
            close(http_response.fd);                                      // 发送完毕后关闭请求文件的文件描述符
        }
    }
    ~EndPoint()
    {
        close(sock); // 析构的时候关闭
    }
};

//#define DEBUG 1
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