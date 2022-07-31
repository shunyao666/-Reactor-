#pragma once

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

#include "Thread.h"
#include "nocopyable.h"

/*
结合EventLoop 和 thread实现one loop per thread 模型 
绑定线程和里面运行的loop
*/
class EventLoop;
class EventLoopThread : nocopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback()
                , const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();
private:

    void threadFunc();
    EventLoop* loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;


};