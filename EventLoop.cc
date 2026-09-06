#include "EventLoop.h"
#include "Channel.h"
#include "Logger.h"
#include "Poller.h"

#include <errno.h>
#include <fcntl.h>
#include <memory>
#include <sys/eventfd.h>
#include <unistd.h>

//防止一个线程创建多个EventLoop
thread_local EventLoop *t_loopInThisThread = nullptr;

//定义默认的Poller IO复用接口的超时时间
const int kPollTimes = 10000;

//创建wakeupfd，用来notify唤醒subReactor处理新来的channel
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d \n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false), quit_(false), callingPendingFunctors_(false),
      threadID_(CurrentThread::tid()), poller_(Poller::newDefaultPoller(this)),
      wakeupFd_(createEventfd()), wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop create %p in thread %d \n", this, threadID_);
    if (t_loopInThisThread) //不为空
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n",
                  t_loopInThisThread, threadID_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    //设置wakeupfd感兴趣的事件类型和发生事件后的相应回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    //每一个EventLoop都将监听wakeupchannel的EPOLLIN读事件了
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

//开启事件循环
void EventLoop::loop()
{
    looping_ = true; //开始循环
    quit_ = false;   //未退出

    LOG_INFO("EventLoop %p start looping \n", this);

    while (!quit_)
    {
        //先清空
        activeChannels_.clear();
        // Poller把发生事件的channel放到activeChannels_里
        pollReturnTime_ = poller_->poll(kPollTimes, &activeChannels_);
        for (Channel *channel : activeChannels_)
        {
            // EventLoop通知Channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        doPendingFunctors(); //处理其他线程交办的事
    }
    LOG_INFO("EventLoop %p stop looping \n", this);
}
//退出事件循环
void EventLoop::quit()
{
    quit_ = true;
    //其他线程调用的quit，先唤醒
    if (!isInLoopThread())
    {
        wakeup();
    }
}

//在当前loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) //在当前的loop线程中执行callback
    {
        cb();
    }
    else //在非当前loop线程中执行cb，需要先唤醒loop所在线程，执行cb
    {
        queueInLoop(cb);
    }
}
//把cb放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(std::move(cb));
    }
    //唤醒需要执行cb的loop所在线程
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup(); //唤醒loop所在线程
    }
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8", n);
    }
}

//用来唤醒loop所在的线程
//向wakeupfd写一个数据,wakeupChannel就会发生读事件，当前loop所在线程就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}

// EventLoop的方法=》Poller的方法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors() //执行回调
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(
            pendingFunctors_); //交换空列表和实际的任务列表，减小锁的粒度
    }

    for (const Functor &functor :
         functors) //这个线程自己执行回调，不妨碍别人派发任务
    {
        functor(); //执行当前loop需要执行的回调操作
    }

    callingPendingFunctors_ = false;
}