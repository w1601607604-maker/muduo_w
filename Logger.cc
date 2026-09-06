#include "Logger.h"
#include "Timestamp.h"

#include <iostream>

//获取日志的唯一实例对象
Logger &Logger::instance()
{
    static Logger logger;
    return logger;
}

/*背后的深层原因（底层逻辑）：
在 C/C++ 的编译器视角中，static
是一个“身兼数职”的关键字，它在不同地方意思完全不同： 在类内部（.h
文件中）：它代表“这个函数属于类本身，不属于对象”。 在普通文件里（.cc
文件中）：它代表“内部链接属性（Internal
Linkage）”，意思是“这个函数被死死封印在当前这个文件里，其他任何文件都找不到它”。
这就产生了严重冲突：你在头文件里把 instance()
设计成单例的全局入口，就是为了让全世界（其他所有文件）都能找到它；结果你在源文件里加个
static，又告诉编译器把它封印起来。编译器觉得你逻辑分裂，只能报错。 结论：在 .h
里写 static 告诉编译器这是静态成员；在 .cc 里直接写 Logger& Logger::instance()
即可，编译器看到 Logger:: 就会自动去头文件里查到它是个静态函数。*/
//设置日志级别
void Logger::setLogLevel(int Level) { LogLevel_ = Level; }
//写日志  [级别信息] time ：msg
void Logger::log(std::string msg)
{
    switch (LogLevel_)
    {
    case INFO:
        std::cout << "[INFO]";
        break;
    case ERROR:
        std::cout << "[ERROR]";
        break;
    case FATAL:
        std::cout << "[FATAL]";
        break;
    case DEBUG:
        std::cout << "[DEBUG]";
        break;
    default:
        break;
    }

    //打印时间和msg
    std::cout << Timestamp::now().toString() << " : " << msg << std::endl;
}
