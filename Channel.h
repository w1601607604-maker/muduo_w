#pragma once

#include "Timestamp.h"
#include "noncopyable.h"

#include <functional>
#include <memory>
/*
Channel理解为通道，在muduo中封装了sockfd和其感兴趣的event，如EPOLLIN，EPOLLOUT事件
还绑定了Poller返回的具体事件
*/
class
    EventLoop; //下面只会用到指针，不会用到对象的具体大小，用前置声明，别用头文件

class Channel : noncopyable
{
  public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>; //只读事件的回调

    Channel(EventLoop *Loop, int fd);
    ~Channel();

    // fd得到Poller通知以后，处理事件，调用相应的回调方法
    void handleEvent(Timestamp receiveTime);

    //设置回调函数对象  把cb变成右值，直接移动，不进行深拷贝
    void setReadCallback(ReadEventCallback cb)
    {
        readCallback_ = std::move(cb);
    }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    //防止Channel被手动remove掉之后还在执行回调函数
    void tie(const std::shared_ptr<void> &obj);

    int fd() const { return fd_; }
    int events() const { return events_; }
    // Poller通过这个接口把内核实际发生的事件给Channel，让Channel能调用对应回调函数
    void set_revents(int revt) { revents_ = revt; }

    //设置fd相应的事件状态
    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    }
    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    }
    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }
    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    }
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }

    //返回fd当前的事件状态
    bool isNoneEvent() const
    {
        return events_ == kNoneEvent;
    } //判断这个Channel（这个fd）是否注册过感兴趣的事件
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() const { return index_; }
    void set_index(int idx) { index_ = idx; }

    // one loop per thread
    EventLoop *ownerLoop() { return loop_; }
    //删除Channel
    void remove();

  private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime); //处理受保护的事件
    static const int kNoneEvent; //属于整个类，而不是某个对象 只读
                                 // epoll的事件标志其实就是整数的掩码
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_; //事件循环
    const int fd_;    // fd,Poller监听的对象
    int events_;      //注册fd感兴趣的事件  输入
    int revents_; // Poller返回的具体发生的事件,实际发生的事件 - 输出
    int index_; // EPollPoller的三个状态，未添加，已填加，删除

    std::weak_ptr<void> tie_;
    bool tied_;

    //因为Channel通道里面可以知道fd最终获知的revents，所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};