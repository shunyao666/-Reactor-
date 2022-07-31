#include "EpollPoller.h"
#include "Logger.h"
#include "Channel.h"


#include <errno.h>
#include <unistd.h>
#include <strings.h>

class EventLoop;
// channel未添加的poller中
const int kNew = -1; // channel的成员index_  = -1
// channel已添加到poller中
const int kAdded = 1;
// channel 从poller中删除
const int kDeleted = 2;


EpollPoller::EpollPoller(EventLoop *loop)
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize) // vector epoll_events
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EpollPoller::~EpollPoller() 
{
    ::close(epollfd_);
}

//重写基类Poller的抽象方法 epoll_wait
Timestamp EpollPoller::poll(int timeoutMs, ChannelList *activeChannels) 
{
    // 实际上应该用LOG_DEBUG输出日志更为合理
    LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());

    // &*events.begin()是vector底层的数组的起始地址
    int numEvents = epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);

        if (numEvents == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s timeouts! \n", __FUNCTION__);
    }
    else
    {
        if (saveErrno != EINTR)
        {
            // 因为别的EventLoop可能发生了错误去修改了errno,此时
            // saveErrno的值不一定是最开始errno的值了，为了适配它的日志系统
            errno = saveErrno;
            LOG_ERROR("EpollPoller::poll() err!");
        }
    }
    return now;


}

// channel update remove => EventLoop updateChannel removeChannel => poller updateChannel removeChannel
// epoll_clt
// 所有的channel 会放在ChannelList中进行管理,注册过的channel会被添加入ChannelMap
/**
 *                 EventLoop => poller.poll
 *         ChannelList        Poller
 *                           ChannelMap <fd, Channel*> epollfd
 */
void EpollPoller::updateChannel(Channel *channel) 
{
    // 获取channel在poller中的状态
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

    if (index == kNew || index == kDeleted)
    {
        // 如果这个channel从未添加到poller中
        if (index == kNew)
        {
            // 将这个channel添加到ChannelMap中
            int fd = channel->fd();
            channels_[fd] = channel;
        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);

    }
    else // channel已经在poller上注册过了
    {
        int fd = channel->fd();
        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

 // epoll_clt
 // 从poller中移除channel的逻辑
void EpollPoller::removeChannel(Channel *channel)
{   
    int fd = channel->fd();
    channels_.erase(fd); // 从ChannelList 中删除

    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, fd);


    int index = channel->index();
    if (index == kAdded)
    {
        // 从epoll中删除
        update(EPOLL_CTL_DEL, channel);
    }

    channel->set_index(kNew); // 从来没有在poller中添加过
}

// 填写活跃的链接,被上层的updateChannel()调用
void EpollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents;++i)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); // EventLoop拿到了它的poller发生事件的channel列表了
    }
}


// 更新channel通道，类似于调用epoll_clt对channel里包含的fd事件 add/mod/del
void EpollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    bzero(&event,sizeof(event));

    int fd = channel->fd();

    event.events = channel->events(); // channel感兴趣的事件
    event.data.fd = fd;
    event.data.ptr = channel; // 把channel绑定在ptr上

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d \n", errno);
        }else
        {
            LOG_FATAL("epoll add/mod error:%d \n", errno);
        }
    }
}