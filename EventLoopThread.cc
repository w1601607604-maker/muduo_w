#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
                                 const std::string name)
    : loop_(nullptr), exiting_(false),
      thread_(std::bind(&EventLoopThread::ThreadFunc, this), name), mutex_(),
      cond_(), callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

EventLoop *EventLoopThread::
    startLoop() //开启线程，创建EventLoop对象，并返回EventLoop对象的指针;
{
    thread_.start(); //开启新线程，执行ThreadFunc函数
    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr)
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }

    return loop;
}

//这个方法是在单独的新线程里面运行的
void EventLoopThread::ThreadFunc()
{
    EventLoop
        loop; //创建一个独立的EventLoop，和上面的线程一一对应one loop per thread
    if (callback_)
    {
        callback_(&loop);
    }
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop(); //由Eventloop的loop进入poller的poll，开始循环

    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}