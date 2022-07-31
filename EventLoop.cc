#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory>
// 防止一个线程创建多个EventLoop, thread_local
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认IO复用接口的超时事件 10s
const int kPollTimeMS = 10000;

// 创建wakeup fd,同来唤醒subreactor处理新来的channel
int createEventFd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d \n ", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventFd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_ );
    }else
    {
        // 第一次创建EventLoop对象
        t_loopInThisThread = this;
    }

    // 设置wakeupFd 的事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个EventLoop都将监听wakeupChannel的 EpollIN事件
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n =  read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8 \n", n);
    }

}

// 开启事件循环,驱动底层的poller去执行poll( epoll_wait)
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping \n", this);
    while(!quit_)
    {
        activeChannels_.clear();
        //监听两类fd,一类是client fd,一类是wakeup fd
        pollReturnTime_= poller_->poll(kPollTimeMS, &activeChannels_);
        for (Channel *channel : activeChannels_)
        {
            // Poller监听哪些channel发生事件了，然后上报给EventLoop,通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环需要的回调操作
        /**
         * IO 线程 mainloop accept后会拿到一个fd,将这个fd打包进一个channel fd <= channel subloop
         * mainloop 事先注册一个回调cb(需要subloop来执行)，wakeup subloop后，执行下面的方法，执行之前mainloop注册的cb操作
         */
        doPendingFunctors();

    }

    LOG_INFO("EventLoop %p stop looping \n", this);
    looping_ = false;
}

//结束事件循环 1.在自己的线程中调用quit 2.在非loop的线程中，调用了loop的quit
void EventLoop::quit()
{
   quit_ = true;

   if (!isInLoopThread()) // 如果是在其他线程中调用了quit, 在一个subloop中，调用了mainloop的quit
   {
       wakeup();
   }
}

// 在当前loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) // 在当前的loop线程中执行callback
    {
        cb();
    }else  // 在非当前的loop线程中执行callback,就需要唤醒loop所在线程执行cb
    {
        queueInLoop(cb);
    }
}

// 把cb放入队列中，唤醒loop所在的线程,执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    // 唤醒相应的，需要执行上面回调操作的线程   
    // callingPendingFunctors_在此处为true的话，说明还在执行回调操作，mainloop给subloop注册了新的回调，处理完后会
    // 会阻塞在loop() => poller_->poll()上，所以需要把当前的loop唤醒，去执行新的callback
    // 
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup(); // 唤醒loop所在线程
    }
}

//唤醒loop所在的线程,向wakeupfd写一个数据,wakeupChannel_就发生读事件，当前loop线程就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop write %lu bytes instead of 8 \n", n);
    }
}

// EventLoop的方法 => Poller的方法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel (Channel *channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}


void EventLoop::doPendingFunctors() // 执行回调
{
    // 这里交换的好处，如果不交换到一个局部变量的话。vector没处理完的话，锁是不能释放的，导致mainloop不能向subloop注册新的functor
    // 导致mainloop阻塞在下发channel的操作上，造成服务器的时延变长
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const Functor &functor : functors)
    {
        functor(); // 执行当前loop需要的回调操作
    }
    callingPendingFunctors_ = false;
}