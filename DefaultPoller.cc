#include "Poller.h"
#include "EpollPoller.h"
#include <stdlib.h>

// 通过这样的一个文件分解了Poller和EPOll / POLL的强耦合
// 派生类可以包含基类的头文件是合理的，基类去包含派生类的头文件是不合理的
Poller* newDefaultPoller(EventLoop *loop)
{
    if (::getenv("MUDUO_USE_POLL"))
    {
        return nullptr;// 生成poll的实例
    }else{
        return new EpollPoller(loop); // 生成epoll的实例
    }

}