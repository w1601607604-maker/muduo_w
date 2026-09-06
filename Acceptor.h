#pragma once

#include "Channel.h"
#include "Socket.h"
#include "noncopyable.h"

#include <functional>

class EventLoop;
class InetAddress;

class Acceptor : noncopyable
{
  public:
    using NewConnectionCallback =
        std::function<void(int sockfd, const InetAddress &)>;

    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(NewConnectionCallback cb)
    {
        newConnectionCallback_ = std::move(cb);
    }
    bool listenning() const { return listening_; }

    void listen();

  private:
    void handleRead(); //有新用户连接时的回调

    EventLoop *loop_; // Acceptor用的就是用户定义的baseloop，也是mainloop
    Socket acceptSocket_;   //监听套接字
    Channel acceptChannel_; //监听套接字对应的channel
    NewConnectionCallback newConnectionCallback_; //有新用户连接时的回调
    bool listening_;                              //是否处于监听状态
};