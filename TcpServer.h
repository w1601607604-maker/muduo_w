#pragma once

#include "Acceptor.h"
#include "Callbacks.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "InetAddress.h"
#include "noncopyable.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

//对外的服务器编程使用的类
class TcpServer : noncopyable
{
  public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    enum Option
    {
        kNoReusePort,
        kReusePort,
    };
    TcpServer(EventLoop *loop, const InetAddress &listenAddr,
              const std::string &nameArg, Option option = kNoReusePort);
    ~TcpServer(); // force out-line dtor, for std::unique_ptr members.

    void setThreadInitCallback(const ThreadInitCallback &cb)
    {
        threadInitCallback_ = cb;
    } // loop线程初始化的回调
    void setConnectionCallback(const ConnectionCallback &cb)
    {
        connectionCallback_ = cb;
    } //有新连接时的回调
    void setMessageCallback(const MessageCallback &cb)
    {
        messageCallback_ = cb;
    } //有读写消息时的回调
    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    {
        writeCompleteCallback_ = cb;
    } //消息发送完成后的回调

    void setThreadNum(int numThreads); //设置底层subloop的个数

    void start(); //开启服务器监听
  private:
    void newConnection(int sockfd, const InetAddress &peerAddr);

    void removeConnection(const TcpConnectionPtr &conn);

    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop *loop_; // baseLoop用户定义的loop

    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor>
        acceptor_; //运行在mainLoop，任务就是监听新连接事件

    std::shared_ptr<EventLoopThreadPool> threadPool_; // one loop per thread

    ConnectionCallback connectionCallback_;       //有新连接时的回调
    MessageCallback messageCallback_;             //有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; //消息发送完成后的回调
    ThreadInitCallback threadInitCallback_;       // loop线程初始化

    std::atomic_int started_;

    int nextConnId_;            //用来给新连接命名的计数器
    ConnectionMap connections_; //保存所有的连接
};