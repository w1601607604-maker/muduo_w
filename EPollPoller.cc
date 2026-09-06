#include "EPollPoller.h"
#include "Channel.h"
#include "Logger.h"

#include <errno.h>
#include <unistd.h>

// Channel未添加到Poller中
const int kNew = -1; // Channel的成员Index初始化就是-1
//已添加到Poller中
const int kAdded = 1;
// Channel从Poller中删除
const int kDeleted = 2;

EPollerPoller::EPollerPoller(EventLoop *loop)
    : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) // vector<epoll_event>
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}
EPollerPoller::~EPollerPoller() { ::close(epollfd_); }

Timestamp EPollerPoller::poll(int timeoutMs, ChannelList *ActiveChannels)
{
    //实际上用LOG_DEBUG更合理，poll调用次数多，debug不显示的时候不会影响函数效率
    LOG_INFO("func=%s => fd total count:%lu\n", __FUNCTION__, channels_.size());

    int numEvents = ::epoll_wait(epollfd_, events_.data(),
                                 static_cast<int>(events_.size()), timeoutMs);

    int saveError = errno; //用局部变量存全局变量,防止errno之后被更改
    Timestamp now(Timestamp::now());
    if (numEvents > 0)
    {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, ActiveChannels);
        if (numEvents ==
            events_.size()) //监听到的所有fd都发生事件，说明可能不够，应该扩容
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s func timeout \n", __FUNCTION__);
    }
    else // 警报！发生错误（负数）
    {
        if (saveError != EINTR) // EINTR是被系统信号打断（比如按下
                                // Ctrl+C）可以忽略。其他错误必须记下来
        {
            errno = saveError;
            LOG_ERROR("EPollerPoller::poll error!");
        }
    }
    return now;
}

// channel通过调用EventLoop的updateChannel和removeChannel来调用Poller的updateChannel和removeChannel，进而调用EPollPoller的

/*

*/
void EPollerPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__,
             channel->fd(), channel->events(), index);
    if (index == kNew || index == kDeleted) //未添加到Poller中
    {
        if (index == kNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else // channel已经在poller上注册过了
    {
        int fd = channel->fd();
        if (channel->isNoneEvent()) // 如果channel对任何事件都不感兴趣
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}
//从Poller中删除channel
void EPollerPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd);
    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, fd);
    int index = channel->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}
//填写活跃的连接
void EPollerPoller::fillActiveChannels(int numEvents,
                                       ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; i++)
    {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(
            channel); // EventLoop拿到了Poller给它返回的所有发生事件的Channel了
    }
}
//更新Channel通道
void EPollerPoller::update(int operation, Channel *channel)
{
    epoll_event event{};
    event.events = channel->events();
    int fd = channel->fd();

    event.data.ptr = channel;

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) //出错了
    {
        if (operation == EPOLL_CTL_DEL) // delete出错可以接受，记录下日志
        {
            LOG_ERROR("epoll_ctl_del error:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl_add/mol  error:%d\n", errno);
        }
    }
}
