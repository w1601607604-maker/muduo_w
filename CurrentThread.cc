#include "CurrentThread.h"

#include <unistd.h>

namespace CurrentThread
{
thread_local int t_cachedTid = 0;

void cacheTid()
{
    if (t_cachedTid == 0)
    {
        /*通过Linux系统调用，获取当前线程的tid
        t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }*/
        t_cachedTid = ::gettid();
    }
}

} // namespace CurrentThread

/*
#include <iostream>
#include <thread>

// 现代 C++ 根本不需要自己手写缓存函数了！
// thread_local 支持复杂的 C++
类，它自己就会在线程第一次启动时自动执行“懒加载”初始化！
thread_local std::thread::id t_myThreadId = std::this_thread::get_id();

void printTid() {
    // 直接使用，底层自动帮你缓存好了，全宇宙跨平台通用
    std::cout << "当前线程ID: " << t_myThreadId << std::endl;
}
*/