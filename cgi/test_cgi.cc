#include <iostream>
#include <cstdlib>
#include <unistd.h>

// 获取数据
bool GetQueryString(std::string &query_string)
{
    bool result = false;
    std::string method = getenv("METHOD"); // 获取方法类型
    if (method == "GET")
    {
        query_string = getenv("QUERY_STRING");
        result = true;
    }
    else if (method == "POST")
    {
        // 需要知道获取多少个字节,通过环境变量来获取
        int content_length = atoi(getenv("CONTENT_LENGTH"));
        // POST方法通过标准输入获取数据
        char ch = 0;
        while (content_length--)
        {
            read(0, &ch, 1);
            query_string.push_back(ch);
        }
        result = true;
    }
    else
    {
        result = false;
    }
    return result;
}

// 分割字符串
void CutString(std::string &in, const std::string &sep, std::string &out1, std::string &out2)
{
    auto pos = in.find(sep);
    if (pos != std::string::npos)
    {
        out1 = in.substr(0, pos);
        out2 = in.substr(pos + sep.size());
    }
}

int main()
{
    std::string query_string;
    GetQueryString(query_string);
    // query_string: a=100&b=100
    std::string str1;
    std::string str2;
    CutString(query_string, "&", str1, str2);
    std::string name1;
    std::string value1;
    CutString(str1, "=", name1, value1);
    std::string name2;
    std::string value2;
    CutString(str2, "=", name2, value2);

    // 传递给父进程
    std::cout << name1 << " : " << value1 << std::endl;
    std::cout << name2 << " : " << value2 << std::endl;

    std::cerr << name1 << " : " << value1 << std::endl;
    std::cerr << name2 << " : " << value2 << std::endl;
    return 0;
}