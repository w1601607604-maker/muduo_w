#pragma once

#include <string>
#include <vector>

//网络库底层的缓冲区类型
class Buffer
{
  public:
    static const size_t kCheapPrepend = 8; //预留空间，方便在头部添加数据
    static const size_t kInitialSize = 1024; //缓冲区的初始大小

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize), readerIndex_(kCheapPrepend),
          writerIndex_(kCheapPrepend)
    {
    }

    //可读数据长度
    size_t readableBytes() const { return writerIndex_ - readerIndex_; }

    size_t writableBytes() const { return buffer_.size() - writerIndex_; }

    size_t prependableBytes() const { return readerIndex_; }

    //返回缓冲区中可读数据的起始地址
    const char *peek() const { return begin() + readerIndex_; }

    void retrieve(size_t len)
    {
        if (len < readableBytes())
        {
            readerIndex_ += len;
        }
        else
        {
            retrieveAll();
        }
    }

    void retrieveAll()
    {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    //把onMessage函数上报的Buffer数据转成string类型的数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes()); //返回缓冲区中所有可读数据
    }

    std::string retrieveAsString(size_t len)
    {
        std::string result(
            peek(), len); //缓冲区中可读数据的起始地址和长度来构造string对象
        retrieve(len); //上一句读完缓冲区，这里对缓冲区复位
        return result;
    }

    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len);
        }
    }

    //把[data, data+len]内存上的数据添加到缓冲区中
    void append(const char *data, size_t len)
    {
        ensureWritableBytes(len); //保证缓冲区有足够的空间写入数据
        std::copy(data, data + len, begin() + writerIndex_);
        writerIndex_ += len;
    }

    char *beginWrite() { return begin() + writerIndex_; }
    const char *beginWrite() const { return begin() + writerIndex_; }

    ssize_t readFd(int fd, int *savedErrno); //从fd上读取数据，保存到缓冲区中

    //通过fd发送数据
    ssize_t writeFd(int fd, int *savedErrno);

  private:
    char *begin()
    {
        return buffer_.data(); //返回vector底层数组的首地址
    }

    const char *begin() const { return buffer_.data(); }

    void makeSpace(size_t len)
    {
        // kCheapPrepend | readableBytes() | writableBytes() = buffer_.size()
        // kCheapPrepend |                    len                      |
        //可写的加上前面空闲的小于需要的
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len); //扩容
        }
        else
        {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_, begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }
    std::vector<char> buffer_; //底层缓冲区
    size_t readerIndex_;       //可读位置
    size_t writerIndex_;       //可写位置
};