#pragma once

#include "Poller.h"
#include "Timestamp.h"

#include <vector>
#include <sys/epoll.h>

/**
 * @brief 
 * epoll的使用
 * epoll_create
 * epoll_clt add modify delete
 * epoll_wait
 */

class Channel;
class EpollPoller : public Poller
{
public:

    // epoll_create
    EpollPoller(EventLoop *loop);
    ~EpollPoller() override;

    //重写基类Poller的抽象方法 epoll_wait
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    // epoll_clt
    void updateChannel(Channel *channel) override;
    // epoll_clt
    void removeChannel(Channel *channel) override;
private:

    static const int kInitEventListSize = 16;

    // 填写活跃的链接,被上层的updateChannel()调用
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    // 更新channel通道，类似于调用epoll_clt对channel里包含的fd事件
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
    

};