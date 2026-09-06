#include "InetAddress.h"

#include <string.h>
#include <strings.h>
InetAddress::InetAddress(uint16_t port, std::string ip /* = "127.0.0.1" */)
{
    // memset(&addr_,0,sizeof(addr_));
    addr_ = {}; //代替memset，直接通过大括号实现全内存安全清零
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = inet_addr(ip.c_str()); // string -> char* 用c_str
}

std::string InetAddress::toIp() const
{
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    //前面加::底层操作系统 API 调用，而不是某个普通的 C++ 对象方法。
    return buf;
}
std::string InetAddress::toIpPort() const
{
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    uint16_t port = ntohs(addr_.sin_port);
    return std::string(buf) + ":" + std::to_string(port);
}
uint16_t InetAddress::toPort() const { return ntohs(addr_.sin_port); }

// #include <iostream>
// int main()
// {
//     InetAddress addr(8080);
//     std::cout << addr.toIpPort() << std::endl;
//     return 0;
// }
