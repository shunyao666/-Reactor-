#pragma once

#include <functional>
#include <memory>

#include "nocopyable.h"
#include "Timestamp.h"

class EventLoop;
/**
 * @brief 
 * EventLoop Channel Poller 之间的关系 Reactor模型上对应Demultiplex
 * 一个事件循环里有很多个channel一个poller
 * channel 理解为通道， 封装了sockfd 和 其他感兴趣的Event,比如EPOLLIN EPOLLOUT事件
 * 还绑定了poller返回的具体事件
 */
class Channel : nocopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // fd得到poller通知以后，处理事件
    void handleEvent(Timestamp receiveTime);

    //设置回调函数对象
    void setReadCallback(ReadEventCallback cb) { readEventCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    //防止当channel被手动remove掉，channel还在执行回调操作
    void tie(const std::shared_ptr<void>&);

    int fd() const { return fd_; }
    int events() const { return events_; }
    // 暴露给poller使用，poller在外部设置具体发生的事件
    int set_revents(int revt) { revents_ = revt; }

    //设置fd相应的事件状态
    void enableReading() { events_ |= KReadEvent; update(); }
    void disableReading() { events_ &= ~KReadEvent; update(); }
    void enbleWriting() { events_ |= KWriteEvent; update(); }
    void disableWriting() { events_ &= ~KWriteEvent; update(); }
    void disableAll() { events_ = KNoneEvent; update(); }

    //返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == KNoneEvent; }
    bool isWriting() const { return events_ & KWriteEvent; }
    bool isReading() const { return events_ & KReadEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // one loop per thread
    EventLoop* ownerLoop() { return loop_; }
    void remove();

private:

    void update();
    void handleEventWithGuard(Timestamp receiveTime);
    
    // 描述了poller感兴趣的事件的状态
    static const int KNoneEvent;
    static const int KReadEvent;
    static const int KWriteEvent;

    EventLoop *loop_; // event loop
    const int fd_; // fd, poller监听的对象
    int events_; // 注册fd感兴趣的事件
    int revents_; // poller返回的具体发生的事件
    int index_;

    std::weak_ptr<void> tie_;
    bool tied_;

    // 因为channel通道里面能够获知fd最终发生的具体的事件的revents, 所以它
    // 负责调用具体事件的回调操作
    ReadEventCallback readEventCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;


};

