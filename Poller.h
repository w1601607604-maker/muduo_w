#pragma once

#include "Timestamp.h"
#include "noncopyable.h"

#include <unordered_map>
#include <vector>

class Channel;
class EventLoop;
// muduo库中多路事件分发器的核心IO复用模块
class Poller : noncopyable
{
  public:
    using ChannelList = std::vector<Channel *>;

    Poller(EventLoop *Loop);
    virtual ~Poller() = default;

    //给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    //判断传进的Channel是否在当前Poller中
    bool hasChannel(Channel *channel) const;

    // EventLoop可以通过该接口获取默认的IO复用的具体实现 epoll或poll
    static Poller *newDefaultPoller(EventLoop *Loop);

  protected:
    // map的key：sockfd  value: sockfd所属的Channel
    using ChannelMap =
        std::unordered_map<int, Channel *>; //将fd和这个Poller管理的Channel对应
    ChannelMap channels_;

  private:
    EventLoop *ownerLoop_; //定义Poller所属的事件循环EventLoop
};