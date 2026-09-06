#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

//类的静态成员变量在类外初始化
std::atomic_int Thread::numCreated_{0}; //统一用大括号直接初始化，省掉拷贝

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false), joined_(false), tid_(0), func_(std::move(func)),
      name_(name)
{
    setDefaultName();
}
Thread::~Thread()
{
    //已经启动并且没有调用join去等他结束
    if (started_ && !joined_)
    {
        thread_->detach(); // thread类设置的提供分离线程的方法
    }
}

void Thread::start()
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);
    //开启线程  用makeshared更好
    thread_ = std::shared_ptr<std::thread>(new std::thread([&]() {
        //获取线程的tid值
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        //开启一个新线程，专门执行线程函数
        func_();
    }));
    //这里必须等待上面新创建的线程获取tid值
    sem_wait(&sem);
}
void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32];
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}
