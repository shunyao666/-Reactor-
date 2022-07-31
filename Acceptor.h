#pragma once

#include <functional>

#include "nocopyable.h"
#include "Socket.h"
#include "Channel.h"

class EventLoop;
class InetAddress;

// Accept是对listenfd的一个封装
// listenfd 用于监听连接请求
class Acceptor : nocopyable
{
public:
    using NewConnectCallback = std::function<void(int sockfd, const InetAddress&)>;
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectCallback &cb)
    {
        newConnectionCallback_ = std::move(cb);
    }

    bool listenning() const { return listenning_; }
    void listen();

private:
    void handleRead();
    
    EventLoop *loop_; // acceptor 用的就是用户定义的baseLoop,也称作mainLoop
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectCallback newConnectionCallback_;
    bool listenning_;
};