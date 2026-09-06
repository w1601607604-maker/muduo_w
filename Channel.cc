#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *Loop, int fd)
    : loop_(Loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{
}
Channel::~Channel()
{
    // assert
}
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}
//当改变Channel所表示的fd后，update负责在poller里更改fd相应的事件epoll_ctl
// EventLoop 含有一个ChannelList（多个Channel） 和 一个Poller
void Channel::update()
{
    //通过Channel所属的EventLoop，调用Poller的方法，注册fd的events事件

    loop_->updateChannel(this);
}
//在Channel所属的EventLoop中把当前的Channel删除掉
void Channel::remove() { loop_->removeChannel(this); }
void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard) ///////////////////////////////////////
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

//根据Poller通知的Channel发生的具体事件，由Channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent revents:%d\n ", revents_);
    //连接挂断并且没有可读数据，就关掉
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_) //如果向closeCallback_这个盒子里注册过回调函数
        {
            closeCallback_();
        }
    }
    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }
    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}