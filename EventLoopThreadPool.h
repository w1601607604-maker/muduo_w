#pragma once

#include "noncopyable.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable
{
  public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    //如果工作在多线程中，loop会默认以轮询的方式分配channel给subloop
    EventLoop *getNextLoop();

    //返回池里所有的loops
    std::vector<EventLoop *> getAllLoops();

    bool started() const { return started_; }

    const std::string name() const { return name_; }

  private:
    EventLoop *baseLoop_; //用户创建的第一个Eventloop对象
    std::string name_;    //线程池名字
    bool started_;        //线程池是否已经启动
    int numThreads_;
    int next_;                                              //轮询的下标
    std::vector<std::unique_ptr<EventLoopThread>> threads_; //包含所有事件线程
    std::vector<EventLoop *> loops_; //包含事件线程的指针
};