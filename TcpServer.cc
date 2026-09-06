#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include <functional>

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainloop is null \n", __FILE__, __FUNCTION__,
                  __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr,
                     const std::string &nameArg, Option option)
    : loop_(CheckLoopNotNull(loop)), ipPort_(listenAddr.toIpPort()),
      name_(nameArg),
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
      threadPool_(new EventLoopThreadPool(loop, name_)), connectionCallback_(),
      messageCallback_(), nextConnId_(1)
{
    //当有新连接时，会执行TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection,
                                                  this, std::placeholders::_1,
                                                  std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for (auto &item : connections_)
    {
        //这个局部的TcpConnectionPtr对象会在离开作用域时析构，调用~TcpConnection()，释放资源
        TcpConnectionPtr conn(item.second);
        item.second.reset(); //重置shared_ptr，减少引用计数

        //调用TcpConnection::connectDestroyed()，销毁连接
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

//设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}
//开启服务器监听
void TcpServer::start()
{
    if (started_++ == 0) //防止一个TcpServer对象被start多次
    {
        threadPool_->start(threadInitCallback_); //开启底层的loop线程池

        loop_->runInLoop(
            std::bind(&Acceptor::listen, acceptor_.get())); // listenfd开始监听
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    EventLoop *ioLoop =
        threadPool_->getNextLoop(); //轮询算法，选择一个subloop来管理channel
    char buf[64]{};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s\n",
             name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    //通过sockfd获取其绑定的本机ip和端口信息，构造InetAddress对象
    sockaddr_in local{};
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }
    InetAddress localAddr(local); //获取新连接的本地地址信息

    //根据连接成功的sockfd，创建TcpConnection对象
    TcpConnectionPtr conn(
        new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn; //将新连接添加到connections_中

    //下面的回调都是用户设置给TcpServer的，通过TcpServer传给了TcpConnection
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    //设置连接关闭的回调，TcpConnection::handleClose()会调用这个回调
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

//在TcpConnection对象上调用connectDestroyed()，销毁连接
// poller => channel::closeCallback = > TcpConnection::handleEvent =>
// handleClose => TcpConnection::connectDestroyed
void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
             name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());

    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}