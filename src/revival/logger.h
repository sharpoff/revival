#pragma once

#include <iostream>
#include <string>

enum LogType {
    LOG_WARNING,
    LOG_ERROR,
    LOG_INFO
};

enum LogColor {
    YELLOW = 33,
    RED = 31,
    WHITE = 39,
    GRAY = 90
};

class Logger
{
public:

    template<class ...Args>
    static void print(LogType type, Args ...args)
    {
        LogColor color = GRAY;
        std::string typeName;

        if (type == LOG_WARNING) {
            color = YELLOW;
            typeName = "WARNING";
        } else if (type == LOG_ERROR) {
            color = RED;
            typeName = "ERROR";
        } else if (type == LOG_INFO) {
            color = GRAY;
            typeName = "INFO";
        }

        std::cout << "\033[" << color << "m" << "[" << typeName << "] " << __TIME__ << ": ";
        (std::cout << ... << args);
        std::cout << "\033[m" << std::endl;
    }
};
