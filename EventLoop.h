#pragma once

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "nocopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"
/**
 * @brief 
 * 相当于Reactor
 * 事件循环类，主要包含了两大模块Channel, Poller(epoll的抽象)
 */

class Poller;
class Channel;

class EventLoop
{
public:
    // 定义了一个回调的类型
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();

    // 退出事件循环
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    // 在当前loop中执行cb
    void runInLoop(Functor cb);

    // 把cb放入队列中，唤醒loop所在的线程执行cb
    void queueInLoop(Functor cb);

    //唤醒loop所在的线程
    void wakeup();

    // EventLoop的方法 => Poller的方法
    void updateChannel(Channel *channel);
    void removeChannel (Channel *channel);
    bool hasChannel(Channel *channel);

    // 判断EventLoop对象是否在自己的线程里面
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
private:

    void handleRead(); // wake up
    void doPendingFunctors(); // 执行回调

    using ChannelList = std::vector<Channel*>;
    std::atomic_bool looping_; // 原子操作，通过CAS实现
    std::atomic_bool quit_; //标志退出loop循环
    
    const pid_t threadId_; //记录当前loop所在线程的ID
    Timestamp pollReturnTime_; //poller返回发生时间的channels的时间点
    std::unique_ptr<Poller> poller_;

    int wakeupFd_; // 当main loop获取一个新用户的channel, 通过轮询算法选择一个subloop,通过该成员唤醒subloop处理channel
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;
  

    std::atomic_bool callingPendingFunctors_; //当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_; // 存储loop需要执行的所有回调操作
    std::mutex mutex_; // 互斥锁，用来保护上面vector容器的线程安全操作
};