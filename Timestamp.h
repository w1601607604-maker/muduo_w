#pragma once

#include <cstdint> //int64_t头文件
#include <string>
class Timestamp
{
  public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch);
    static Timestamp now();       //获取现在时间
    std::string toString() const; //打印年月日

  private:
    int64_t microSecondsSinceEpoch_;
};