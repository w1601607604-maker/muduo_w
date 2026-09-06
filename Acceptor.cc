#include "Acceptor.h"
#include "Channel.h"
#include "InetAddress.h"
#include "Logger.h"
#include "Socket.h"

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                          IPPROTO_TCP);
    if (sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d\n", __FILE__,
                  __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr,
                   bool reuseport)
    : loop_(loop), acceptSocket_(createNonblocking()),
      acceptChannel_(loop, acceptSocket_.fd()), listening_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr); //绑定socket
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading(); //把acceptChannel_注册到poller里
}

// listenfd有事件发生了，有新用户连接了
void Acceptor::handleRead()
{
    InetAddress peerAddr; // peerAddr用来接收新连接的客户端地址和端口
    int connfd =
        acceptSocket_.accept(&peerAddr); // accept返回新连接的socket描述符
    if (connfd >= 0)
    {
        if (newConnectionCallback_)
        {
            newConnectionCallback_(
                connfd,
                peerAddr); //轮询找到一个subloop，唤醒，分发当前的connfd给它
        }
        else //没有回调，无法执行操作
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept create err:%d\n", __FILE__, __FUNCTION__,
                  __LINE__, errno);
        if (errno == EMFILE) //资源用完了
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit create err:%d\n", __FILE__,
                      __FUNCTION__, __LINE__, errno);
        }
    }
}
