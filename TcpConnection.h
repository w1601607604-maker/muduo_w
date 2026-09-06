#pragma once

#include "Buffer.h"
#include "Callbacks.h"
#include "InetAddress.h"
#include "Timestamp.h"
#include "noncopyable.h"

#include <atomic>
#include <memory>
#include <string>

class Channel;
class EventLoop;
class Socket;

class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection>
{
  public:
    TcpConnection(EventLoop *loop, const std::string &nameArg, int sockfd,
                  const InetAddress &localAddr, const InetAddress &peerAddr);
    ~TcpConnection();

    EventLoop *getLoop() const { return loop_; }
    const std::string &name() const { return name_; }
    const InetAddress &localAddress() const { return localAddr_; }
    const InetAddress &peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    void send(const std::string &buf); //发送数据
    void send(Buffer *buf);            //发送数据
    void shutdown();                   //关闭连接

    void setTcpNoDelay(bool on);

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
    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb,
                                  size_t highWaterMark)
    {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    } //高水位回调
    void setCloseCallback(const CloseCallback &cb)
    {
        closeCallback_ = cb;
    } //关闭连接的回调

    void connectEstablished(); //连接建立
    void connectDestroyed();   //连接销毁

  private:
    enum StateE
    {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    }; // TcpConnection的状态

    void setState(StateE state) { state_ = state; }

    void handleRead(Timestamp receiveTime); //处理读事件
    void handleWrite();                     //处理写事件
    void handleClose();                     //处理关闭事件
    void handleError();                     //处理错误事件

    // void sendInLoop(const std::string &buf);          //发送数据
    void sendInLoop(const void *message, size_t len); //发送数据

    void shutdownInLoop(); //关闭连接

    EventLoop *loop_;        // TcpConnection属于某个subLoop,不是mainLoop
    const std::string name_; // TcpConnection的名字
    std::atomic_int state_;  // TcpConnection的状态
    bool reading_;           //是否在读数据

    std::unique_ptr<Socket> socket_;   // TcpConnection的socket
    std::unique_ptr<Channel> channel_; // TcpConnection的channel

    const InetAddress localAddr_; // TcpConnection的本地地址
    const InetAddress peerAddr_;  // TcpConnection的对端地址

    ConnectionCallback connectionCallback_;       //有新连接时的回调
    MessageCallback messageCallback_;             //有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; //消息发送完成后的回调
    HighWaterMarkCallback highWaterMarkCallback_; //高水位回调
    CloseCallback closeCallback_;                 //关闭连接的回调]

    size_t highWaterMark_; //高水位阈值

    Buffer inputBuffer_;  //接收数据的缓冲区
    Buffer outputBuffer_; //发送数据的缓冲区
};