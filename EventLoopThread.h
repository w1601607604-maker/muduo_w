#pragma once

#include "Thread.h"
#include "noncopyable.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

class EventLoop;

class EventLoopThread : noncopyable
{
  public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string name = {});
    ~EventLoopThread();

    EventLoop *
    startLoop(); //开启线程，创建EventLoop对象，并返回EventLoop对象的指针;

  private:
    void ThreadFunc();

    EventLoop *loop_;
    bool exiting_; //是否退出循环
    Thread thread_;

    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};