// 工具类

#include <string>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>

class Util
{
public:
    // 读取一整行
    static int ReadLine(int sock, std::string &out)
    {
        char ch = 'X';

        while (ch != '\n') // 按照\n来分隔
        {
            ssize_t s = recv(sock, &ch, 1, 0);
            if (s > 0)
            {
                if (ch == '\r')
                {
                    // recv的操作是从接收缓冲区中直接取走一个字符
                    // 需要查看\r后面的字符，但是不能取走
                    recv(sock, &ch, 1, MSG_PEEK); // 通过MSG_PEEK选项窥探下一个字符但是不取走
                    if (ch == '\n')
                    {
                        // 如果下一个字符为\n
                        recv(sock, &ch, 1, 0); // 直接取走，将\r转换为\n
                    }
                    else
                    {
                        ch = '\n';
                    }
                }
                // 1.\n
                // 2.普通字符
                out.push_back(ch); // 将ch加入到到out中
            }
            else if (s == 0)
                return 0;
            else
                return -1;
        }
        return out.size(); // 返回读入到的数据个数
    }

    // 分割字符串 seq表示分隔符
    static bool CutString(const std::string &target, std::string &key_out, std::string &value_out, std::string seq)
    {
        size_t pos = target.find(seq);
        if (pos != std::string::npos)
        {
            key_out = target.substr(0, pos); // substr (]前闭后开区间
            value_out = target.substr(pos + seq.size());
            return true;
        }
        return false;
    }
};