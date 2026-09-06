#pragma once

#include <string>

#include "noncopyable.h"

// LOG_INFO("%s,%d, arg1,arg2")
#define LOG_INFO(logmsgFormat, ...)                                            \
    do                                                                         \
    {                                                                          \
        Logger &logger = Logger::instance();                                   \
        logger.setLogLevel(INFO);                                              \
        char buf[1024] = {0};                                                  \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);                      \
        logger.log(buf);                                                       \
    } while (0)
//宏函数每行都要加\          ##__VA_ARGS__接受可变参数
// do while(0) 防御
#define LOG_ERROR(logmsgFormat, ...)                                           \
    do                                                                         \
    {                                                                          \
        Logger &logger = Logger::instance();                                   \
        logger.setLogLevel(ERROR);                                             \
        char buf[1024] = {0};                                                  \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);                      \
        logger.log(buf);                                                       \
    } while (0)

#define LOG_FATAL(logmsgFormat, ...)                                           \
    do                                                                         \
    {                                                                          \
        Logger &logger = Logger::instance();                                   \
        logger.setLogLevel(FATAL);                                             \
        char buf[1024] = {0};                                                  \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);                      \
        logger.log(buf);                                                       \
        exit(-1);                                                              \
    } while (0)

//#define MUDEBUG

#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...)                                           \
    do                                                                         \
    {                                                                          \
        Logger &logger = Logger::instance();                                   \
        logger.setLogLevel(DEBUG);                                             \
        char buf[1024] = {0};                                                  \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);                      \
        logger.log(buf);                                                       \
    } while (0)
#else
#define LOG_DEBUG(logmsgFormat, ...)
#endif

//定义日志的级别 INFO  正常流程的输出  ERROR 不影响软件正常进行
// FATAL 影响正常运行的错误  DEBUG 调试信息
enum LogLevel
{
    INFO,  //普通信息
    ERROR, //错误信息
    FATAL, // core信息
    DEBUG, //调试信息
};

//输出一个日志类
class Logger : noncopyable
{
  public:
    //获取日志的唯一实例对象
    static Logger &instance();
    // static表明调用这个函数时候可以不用对象obj.log("hello")，直接类名加作用域即可调用
    // 外部代码不需要先有对象，直接这样就能拿到皇帝的圣旨：Logger::instance()->log("出错了！");

    //设置日志级别
    void setLogLevel(int Level);
    //写日志
    void log(std::string msg);

  private:
    int LogLevel_; //枚举类型是整数
    Logger() {} //构造函数私有化，保证单例模式，防止别处创建对象
};
