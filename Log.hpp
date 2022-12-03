// 日志
#pragma once

#include <iostream>
#include <string>
#include <ctime>

#define INFO 1
#define WARRING 2
#define ERROR 3
#define FATAL 4
#define LOG(level, message) Log(#level, message, __FILE__, __LINE__)

// 日志的格式: [日志级别][时间戳][日志信息][错误文件名称][错误的行数]
void Log(std::string level, std::string message, std::string file_name, int line)
{
    std::cout << "[" << level << "]"
              << "[" << time(nullptr) << "]"
              << "[" << message << "]"
              << "[" << file_name << "]"
              << "[" << line << "]" << std::endl;
}