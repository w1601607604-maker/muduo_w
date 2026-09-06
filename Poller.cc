#include "Poller.h"
#include "Channel.h"

Poller::Poller(EventLoop *Loop) : ownerLoop_(Loop) //记录Poller所属的EventLoop
{
}
//确认这个Channel属于这个Poller
bool Poller::hasChannel(Channel *channel) const
{
    auto it = channels_.find(channel->fd());
    //找到了这个fd并且它对应的channel和传进的channel相同
    return it != channels_.end() && it->second == channel;
}