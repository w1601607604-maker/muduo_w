#include "TcpConnection.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Socket.h"

#include <errno.h>
#include <functional>
#include <netinet/tcp.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d Tcpconnection loop is null \n", __FILE__,
                  __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, const std::string &nameArg,
                             int sockfd, const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop)), name_(nameArg), state_(kConnecting),
      reading_(true), socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)), localAddr_(localAddr),
      peerAddr_(peerAddr), highWaterMark_(64 * 1024 * 1024) // 64M
{
    //下面给channel设置回调函数，poller给channel通知感兴趣的事件发生了，channel负责调用回调
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true); //设置底层的socket为心跳保活
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d\n", name_.c_str(),
             channel_->fd(), (int)state_);
}

void TcpConnection::send(const std::string &message)
{
    if (state_ == kConnected) //如果连接状态是已连接
    {
        if (loop_->isInLoopThread()) //如果当前线程是subLoop线程
        {
            sendInLoop(message.c_str(), message.size()); //发送数据
        }
        else //如果当前线程不是subLoop线程
        {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this,
                                       message.c_str(), message.size()));
        }
    }
}
//发送数据，应用写得快，内核发送慢，需要把待发送数据放入缓冲区，而且设置了水位回调

void TcpConnection::sendInLoop(const void *message, size_t len)
{
    ssize_t nwrote = 0;     //已发送字节数
    size_t remaining = len; //剩余
    bool faultError = false;
    if (state_ == kDisconnected) //如果连接状态是已断开，之前调用过shutdown
    {
        LOG_ERROR("disconnected, give up writing");
        return;
    }
    //如果channel感兴趣的事件不是可写
    // channel第一次写数据时，outputBuffer_中没有数据，直接调用write系统调用把数据发送出去
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), message, len); //发送数据
        if (nwrote >= 0)                                //发送成功
        {
            remaining = len - nwrote; //计算剩余未发送的数据长度
            //如果剩余未发送的数据长度为0，说明数据已经全部发送完成
            if (remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else //发送失败
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK) //如果不是资源暂时不可用错误
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if (errno == EPIPE ||
                    errno == ECONNRESET) //如果是管道破裂或连接重置错误
                {
                    faultError = true; //设置故障错误标志为true
                }
            }
        }
    }
    if (!faultError && remaining > 0) //如果没有故障错误且还有剩余未发送的数据
    {
        size_t oldLen =
            outputBuffer_.readableBytes(); //获取发送缓冲区中待发送数据的长度
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ &&
            highWaterMarkCallback_) //如果超过高水位阈值且之前没有超过高水位阈值且设置了高水位回调
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_,
                                         shared_from_this(),
                                         oldLen + remaining));
        }
        outputBuffer_.append((char *)message + nwrote,
                             remaining); //把剩余未发送的数据添加到发送缓冲区中
        if (!channel_->isWriting()) //如果channel感兴趣的事件不是可写
        {
            channel_->enableWriting(); //设置channel感兴趣的事件为可写
        }
    }
}

void TcpConnection::shutdown()
{
    if (state_ == kConnected) //如果连接状态是已连接
    {
        setState(kDisconnecting); //设置连接状态为正在断开
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop, this)); //关闭连接
    }
}

void TcpConnection::shutdownInLoop()
{
    if (!channel_
             ->isWriting()) //如果channel感兴趣的事件不是可写,说明发送缓冲区中没有数据了，说明数据已经全部发送完成
    {
        socket_->shutdownWrite(); //关闭写端
    }
}

//建立连接
void TcpConnection::connectEstablished()
{
    setState(kConnected); //设置连接状态为已连接
    channel_->tie(
        shared_from_this()); //将channel和TcpConnection绑定在一起，防止channel被手动remove掉，TcpConnection被reset掉，导致channel访问野指针
    channel_->enableReading(); //设置channel感兴趣的事件为可读
    connectionCallback_(shared_from_this()); //执行连接建立的回调操作
}

void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected) //如果连接状态是已连接
    {
        setState(kDisconnected); //设置连接状态为已断开
        channel_->disableAll();  //取消channel所有感兴趣的事件
        connectionCallback_(shared_from_this()); //执行连接关闭的回调操作
    }
    channel_->remove(); //从poller中删除channel
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0) //有数据
    {
        //已建立连接的用户有读写消息时，调用用户传入的回调操作
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)
    {
        //客户端断开
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}
void TcpConnection::handleWrite()
{
    if (channel_->isWriting()) //如果channel感兴趣的事件是可写
    {
        int savedErrno = 0;
        //从outputBuffer_的可读数据起始处，往外写出 readableBytes() 个字节到 fd
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if (n > 0) //发送成功
        {
            outputBuffer_.retrieve(n); //把发送缓冲区中已经发送的数据删除掉
            if (outputBuffer_.readableBytes() ==
                0) //发送缓冲区中没有数据了，说明发送完成了
            {
                channel_->disableWriting(); //取消关注可写事件
                if (writeCompleteCallback_)
                {
                    //唤醒loop所在线程，执行回调操作
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting) //如果连接状态是正在断开
                {
                    shutdownInLoop(); //关闭连接
                }
            }
        }
        else //发送失败
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else //感兴趣的事件不是可写，说明写缓冲区发送完了
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing\n",
                  channel_->fd());
    }
}
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n", channel_->fd(),
             (int)state_);
    setState(kDisconnected); //设置连接状态为已断开

    channel_->disableAll(); //取消channel所有感兴趣的事件
    // TcpConnection被关闭了，poller不再关注这个channel了
    // 关闭连接的回调操作
    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr); //执行连接关闭的回调操作
    //必须在用户的回调操作之后执行，因为用户的回调操作中可能会调用到TcpConnection的析构函数
    closeCallback_(shared_from_this());
}
void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) <
        0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n",
              name_.c_str(), err);
}