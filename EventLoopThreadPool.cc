#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"

#include <memory>

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop,
                                         const std::string &nameArg)
    : baseLoop_(baseLoop), name_(nameArg), started_(false), numThreads_(0),
      next_(0)
{
}
EventLoopThreadPool::~EventLoopThreadPool() {}

void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true;
    for (int i = 0; i < numThreads_; ++i)
    {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf); //创建
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(
            t->startLoop()); //底层创建线程，绑定一个新的EventLoop，并返回该loop的地址
    }
    //没有调用setthreadnum，整个服务端只有一个线程，运行着baseloop
    if (numThreads_ == 0 && cb) //线程的初始化回调不为空
    {
        cb(baseLoop_); //执行回调
    }
}

//如果工作在多线程中，loop会默认以轮询的方式分配channel给subloop
EventLoop *EventLoopThreadPool::getNextLoop()
{
    EventLoop *loop = baseLoop_;

    if (!loops_.empty())
    {
        loop = loops_[next_];
        ++next_;
        if (next_ >= loops_.size())
        {
            next_ = 0;
        }
    }
    return loop;
}

//返回池里所有的loops
std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty()) //只有一个baseLoop
    {
        return std::vector<EventLoop *>(1, baseLoop_); //直接构建并返回
    }
    else
    {
        return loops_;
    }
}