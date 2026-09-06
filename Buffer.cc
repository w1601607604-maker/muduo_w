#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h> //readv
#include <unistd.h>

//从fd读取数据  Poller工作在LT模式
// buffer缓冲区有大小，但是从fd读数据的时候，不知道tcp数据的大小

ssize_t Buffer::readFd(int fd, int *savedErrno)
{
    char extrabuf[65536]; //栈上分配64k的空间
    struct iovec vec[2];

    const size_t writable = writableBytes(); //缓冲区中可写的空间大小
    vec[0].iov_base = begin() + writerIndex_; //缓冲区中可写的起始地址
    vec[0].iov_len = writable; //缓冲区中可写的空间大小

    vec[1].iov_base = extrabuf;       //栈上分配的空间
    vec[1].iov_len = sizeof extrabuf; //栈上分配的空间大小
    //当缓冲区中可写的空间不够时，栈上分配的空间就会被使用

    const int iovcnt =
        (writable < sizeof extrabuf)
            ? 2
            : 1; //如果缓冲区中可写的空间小于栈上分配的空间，就使用两个缓冲区，否则只使用一个缓冲区

    const ssize_t n = ::readv(fd, vec, iovcnt); //从fd上读取数据，保存到缓冲区中

    if (n < 0)
    {
        *savedErrno = errno; //保存错误码
    }
    else if (static_cast<size_t>(n) <= writable)
    {
        writerIndex_ += n; //缓冲区中可写的空间足够，直接写入缓冲区
    }
    else // extrabuf中的数据也被写入缓冲区
    {
        writerIndex_ = buffer_.size(); //缓冲区中可写的空间不够，先把缓冲区写满
        append(extrabuf, n - writable); //再把栈上分配的空间中的数据写入缓冲区
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int *savedErrno)
{
    //从 Buffer 的可读数据起始处，往外写出 readableBytes() 个字节到 fd
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *savedErrno = errno; //保存错误码
    }
    return n;
}