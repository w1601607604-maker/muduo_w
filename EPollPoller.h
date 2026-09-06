#pragma once

#include "Poller.h"
#include "Timestamp.h"

#include <sys/epoll.h>
#include <vector>

class Channel;

/*
epoll的使用
epoll_create    创建出fd
epoll_ctl add/mol/del  添加想让epoll监听的fd，以及针对这个fd感兴趣的事件
epoll_wait
*/
class EPollerPoller : public Poller
{
  public:
    EPollerPoller(EventLoop *loop);
    ~EPollerPoller() override;

    //重写基类的抽象方法
    Timestamp poll(int timeoutMs, ChannelList *ActiveChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

  private:
    // EventList的初始大小
    static const int kInitEventListSize = 16;
    //填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    //更新Channel通道
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;
    int epollfd_;
    EventList events_;
};