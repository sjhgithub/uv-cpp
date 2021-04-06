/*
   Copyright © 2017-2019, orcaer@yeah.net  All rights reserved.

   Author: orcaer@yeah.net

   Last modified: 2019-12-31

   Description: https://github.com/wlgq2/uv-cpp
*/

#ifndef UV_TCP_CONNECTION_HPP
#define UV_TCP_CONNECTION_HPP

#include <memory>


#include <chrono>
#include <functional>
#include <atomic>
#include <string>

#include "EventLoop.hpp"
#include "ListBuffer.hpp"
#include "CycleBuffer.hpp"
#include "SocketAddr.hpp"

namespace uv
{

struct WriteInfo
{
	static const int Disconnected = -1;
	int status;
	char* buf;
	unsigned long size;
};


class TcpConnection ;
class TcpServer;
class ConnectionWrapper;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using AfterWriteCallback =  std::function<void (WriteInfo& )> ;
using OnMessageCallback =  std::function<void (TcpConnectionPtr,const char*,ssize_t)>  ;
using OnCloseCallback =  std::function<void (std::string& )>  ;
using CloseCompleteCallback =  std::function<void (std::string&)>  ;


class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public :
    TcpConnection(EventLoop* loop, std::string& name, uv_tcp_t* client, bool isConnected = true, bool bAutoStartReading = true);
    virtual ~TcpConnection();
    
    void onSocketClose();
    //调用uv_close后,该连接会处于FIN_WAIT_1状态,确保数据发送完毕,除非断网或者对端断开
    void close(std::function<void(std::string&)> callback);

    //WSASend will be called with buf's address
    int write(const char* buf,ssize_t size,AfterWriteCallback callback);
    void writeInLoop(const char* buf,ssize_t size,AfterWriteCallback callback);

    void setWrapper(std::shared_ptr<ConnectionWrapper> wrapper);
    std::shared_ptr<ConnectionWrapper> getWrapper();

    void setMessageCallback(OnMessageCallback callback);
    void setConnectCloseCallback(OnCloseCallback callback);
    
    void setConnectStatus(bool status);
    bool isConnected();
    
    const std::string& getName();
    void setName(const std::string&);

    PacketBufferPtr getPacketBuffer();
public:
    UINT64 bytesReceived = 0;
    UINT64 bytesSent = 0;
    bool autoStartReading;
    void* userdata;
    void* parent;//Point to TcpServer or TcpClient;
    void startReading();
    void setDbgRawMesageReceive(bool b = true) { dbgRawMesageReceive_ = b; }
    void setClient(uv_tcp_t* client);
private:
    void onMessage(const char* buf, ssize_t size);
    void closeComplete();
    char* resizeData(size_t size);
    static void  onMesageReceive(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf);
    
private :
    that* that_;
    std::string name_;
    bool connected_;
    EventLoop* loop_;
    uv_tcp_t* handle_;
    std::string data_;
    PacketBufferPtr buffer_;
    std::weak_ptr<ConnectionWrapper> wrapper_;
    FILE* fpDbgLogger_ = nullptr;
    bool dbgRawMesageReceive_ = false;

    OnMessageCallback onMessageCallback_;
    OnCloseCallback onConnectCloseCallback_;
    CloseCompleteCallback closeCompleteCallback_;

    typedef struct goneCallbackPara
    {
        std::function<void(std::string&)> callback;
        std::string name;
    }GONECALLBACKPARA;
    GONECALLBACKPARA* goneCallbackPara_;

};

class  ConnectionWrapper : public std::enable_shared_from_this<ConnectionWrapper>
{
public:
    ConnectionWrapper(TcpConnectionPtr connection)
        :connection_(connection)
    {
    }

    ~ConnectionWrapper()
    {
        TcpConnectionPtr connection = connection_.lock();
        if (connection)
        {
            connection->onSocketClose();
        }
    }

private:
    std::weak_ptr<TcpConnection> connection_;
};
using ConnectionWrapperPtr = std::shared_ptr<ConnectionWrapper>;
}
#endif
