#include <functional>
#include <iostream>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <string>
using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders;
/*基于muduo网络库开发服务器程序
1.组合TcpServer对象
2.创建EventLoop事件循环对象的指针
3.明确TcpServer构造函数需要什么参数，输出ChatServer的构造函数
4.在当前服务器的构造函数中，注册处理连接和读写事件的回调函数
5.设置合适的服务端线程数量，muduo库会自己划分I/O线程和worker线程
主要 写onConnection和onMessage！！！
*/
class ChatServer //基本只有这个类名称可以改，别的大体都不用改
{
  public:
    ChatServer(EventLoop *loop,               //事件循环
               const InetAddress &listenAddr, // IP+Port
               const string &nameArg)         //服务器的名字
        : _server(loop, listenAddr, nameArg), _loop(loop)
    {
        //给服务器注册用户连接和断开的回调函数
        _server.setConnectionCallback(
            std::bind(&ChatServer::onConnection, this, _1));
        //给服务器注册用户读写事件回调
        _server.setMessageCallback(
            std::bind(&ChatServer::onMessage, this, _1, _2, _3));
        //设置服务器端的线程数量
        _server.setThreadNum(5); // 1个I/O线程
    };
    //开启事件循环
    void start() { _server.start(); }

  private:
    //专门处理用户的连接和断开
    void onConnection(const TcpConnectionPtr &conn)
    {

        if (conn->connected())
        {
            cout << conn->peerAddress().toIpPort()
                 << "->" //客户端（用户）的 IP 和端口号
                 << conn->localAddress().toIpPort() << "state:online"
                 << endl; //自己的服务器的 IP 和端口号
        }
        else
        {
            cout << conn->peerAddress().toIpPort() << "->"
                 << conn->localAddress().toIpPort() << "state:offline" << endl;
            conn->shutdown(); // close(fd);
            //_loop->quit();//服务器退出
        }
    }
    //专门处理用户的读写事件
    void onMessage(
        const TcpConnectionPtr &conn, //连接，信息在这里面，通过连接发送数据
        Buffer *buffer, //缓冲区 接受到用户事件的话，从这里面读数据
        Timestamp time) //接收到数据的时间信息
    {
        string buf = buffer->retrieveAllAsString(); //把接收到的数据放到字符串里
        cout << "recv data:" << buf << " time:" << time.toString() << endl;
        conn->send(buf);
    }
    TcpServer _server;
    EventLoop *_loop; // epoll
};

int main()
{
    EventLoop loop;
    InetAddress addr("127.0.0.1", 6000);
    ChatServer server(&loop, addr, "ChatServer");

    server.start(); //把listenfd通过epoll_ctl添加到epoll上
    loop.loop(); // epoll_wait以阻塞方式 等新用户连接，读写事件等
    return 0;
}