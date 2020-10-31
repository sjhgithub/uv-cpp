/*
   Copyright © 2017-2019, orcaer@yeah.net  All rights reserved.

   Author: orcaer@yeah.net

   Last modified: 2019-12-31

   Description: https://github.com/wlgq2/uv-cpp
*/

#ifndef UV_TCP_SERVER_HPP
#define UV_TCP_SERVER_HPP

#include <functional>
#include <memory>
#include <set>
#include <map>

#include "TcpAccepter.hpp"
#include "TcpConnection.hpp"
#include "TimerWheel.hpp"

namespace uv
{

using OnConnectionStatusCallback = std::function<void(TcpConnectionPtr)>;

//no thread safe.
class TcpServer
{
public:
    static void SetBufferMode(uv::GlobalConfig::BufferMode mode);
public:
    TcpServer(EventLoop* loop, bool tcpNoDelay = true, bool bAutoStartReading = true);
    virtual ~TcpServer();
    int bindAndListen(SocketAddr& addr);
	void close(DefaultCallback callback);
    
    std::map<std::string, TcpConnectionPtr>& getConnnections();
    TcpConnectionPtr getConnnection(const std::string& name);
    void closeConnection(const std::string& name);
    void closeConnectionPtr(TcpConnectionPtr connection);
    void StartReading(TcpConnectionPtr connection);
    void StartReading(std::string& name);

    void setNewConnectCallback(OnConnectionStatusCallback callback);
    void setConnectCloseCallback(OnConnectionStatusCallback callback);

    void setMessageCallback(OnMessageCallback callback);

    void write(TcpConnectionPtr connection,const char* buf,unsigned int size, AfterWriteCallback callback = nullptr);
    void write(std::string& name,const char* buf,unsigned int size, AfterWriteCallback callback =nullptr);
    void writeInLoop(TcpConnectionPtr connection,const char* buf,unsigned int size,AfterWriteCallback callback);
    void writeInLoop(std::string& name,const char* buf,unsigned int size,AfterWriteCallback callback);
    //每次TcpConnectionPtr有活动时(初次Accept和每一次读写数据),都按序号加入TimerWheel中的一个集合(有多个集合)
    //该TimerWheel调用一个Timer使用loop线程进行轮询,一秒一次,每次会clear序号的下一个集合
    //这样TcpConnectionPtr就会减去一次引用,减到0就释放
    void setTimeout(unsigned int);
private:
    void onAccept(EventLoop* loop, uv_tcp_t* client);

    void addConnnection(std::string& name, TcpConnectionPtr connection);
    void removeConnnection(std::string& name);
    void onMessage(TcpConnectionPtr connection, const char* buf, ssize_t size);
protected:
    EventLoop* loop_;
private:
    that* that_;
    bool tcpNoDelay_;
    SocketAddr::IPV ipv_;
    std::shared_ptr <TcpAccepter> accetper_;
    std::map<std::string ,TcpConnectionPtr>  connnections_;
    std::mutex muxConnections_;
    bool m_bAutoStartReading;

    OnMessageCallback onMessageCallback_;
    OnConnectionStatusCallback onNewConnectCallback_;
    OnConnectionStatusCallback onConnectCloseCallback_;
    TimerWheel<ConnectionWrapper> timerWheel_;
};


}
#endif
