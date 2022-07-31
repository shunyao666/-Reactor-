#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::KNoneEvent = 0;
const int Channel::KReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::KWriteEvent = EPOLLOUT;

// EventLoop : ChannelLists Poller
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{

}

Channel::~Channel()
{

}

// channel的tie方法什么时候调用过？ 一个TcpConnection新连接创建的时候   TcpConnection底层管理了一个channel，
void Channel::tie(const std::shared_ptr<void>& obj)
{
    tie_ = obj;
    tied_ = false;
}

/**
 * @brief 
 * 当改变Channel所表示的fd的events事件后，update负责在poller里面更改fd相应的事件epoll_ctl
 * EventLoop => ChannelLists poller
 */
void Channel::update()
{
    // 通过channel所属的EventLoop,调用Poller相应的方法,注册fd相应的events事件
   
    loop_->updateChannel(this);
}

// 在channel所属的EventLoop中，把当前的channel删除掉
void Channel::remove()
{
    
     loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }else
    {
        handleEventWithGuard(receiveTime);
    }
}

// 根据poller通知channel发生的具体事件，由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("Channel handleEvents revents:%d\n", revents_);

    // 发生了异常
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
    }

    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }

    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readEventCallback_)
        {
            readEventCallback_(receiveTime);
        }
    }

    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}