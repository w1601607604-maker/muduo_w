#pragma once

namespace CurrentThread
{
extern thread_local int t_cachedTid;

void cacheTid();

inline int tid()
{
    if (__builtin_expect(
            t_cachedTid == 0,
            0)) //还没获取当前线程的id 分支预测，将else的汇编写到前面
    {
        cacheTid();
    }
    return t_cachedTid;
}
} // namespace CurrentThread
