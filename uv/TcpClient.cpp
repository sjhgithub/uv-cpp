/*
   Copyright © 2017-2020, orcaer@yeah.net  All rights reserved.

   Author: orcaer@yeah.net

   Last modified: 2019-12-31

   Description: https://github.com/wlgq2/uv-cpp
*/

#include <string>

#include "include/TcpClient.hpp"
#include "include/LogWriter.hpp"
#include "include/Packet.hpp"

using namespace uv;
using namespace std;


TcpClient::TcpClient(EventLoop* loop, bool tcpNoDelay, bool bAutoStartReading)
    :that_(new that(this)),
    loop_(loop),
    connect_(new uv_connect_t()),
    ipv(SocketAddr::Ipv4),
    tcpNoDelay_(tcpNoDelay),
    connectCallback_(nullptr),
    onMessageCallback_(nullptr),
    connection_(nullptr),
    m_bAutoStartReading(bAutoStartReading)
{
    connect_->data = static_cast<void*>(that_);
    //20200404 by song
    //由于端口转发软件创建TcpClient对象时需要取得TcpConnection对象句柄
    //所以由原来的建立连接时创建改为构造时创建
    update();//新建socket_句柄
    string name = "";
    connection_ = make_shared<TcpConnection>(loop_, name, socket_, false, m_bAutoStartReading);
    connection_->parent = this;
}

TcpClient::~TcpClient()
{
    that_->setWillGoner();
    that_->setGoneCallback(connect_,
        [](void* hpara, void* lpara) -> int {
            uv_connect_t* connect_ = (uv_connect_t*)hpara;
            delete connect_;
            return 1;
        });
    close(nullptr);
}

bool uv::TcpClient::isTcpNoDelay()
{
    return tcpNoDelay_;
}

void uv::TcpClient::setTcpNoDelay(bool isNoDelay)
{
    tcpNoDelay_ = isNoDelay;
}

void TcpClient::StartReading()
{
    if (connection_)
    {
        connection_->startReading();
    }
}
void TcpClient::connect(SocketAddr& addr)
{
    //update();
    ipv = addr.Ipv();    
    ::uv_tcp_connect(connect_, socket_, addr.Addr(),
        [](uv_connect_t* req, int status)
        {
            that* that_ = static_cast<that*>(req->data);
            if (!that_->getWillGoner()) {
                auto handle = static_cast<TcpClient*>((that_->body()));
                if (0 != status)
                {
                    uv::LogWriter::Instance()->error("connect fail.");
                    handle->onConnect(false);
                }
                else {
                    handle->onConnect(true);
                }
                that_->rtnWillGoner();
            }
        });
}

void TcpClient::onConnect(bool successed)
{
    if(successed)
    {
        string name;
        SocketAddr::AddrToStr(socket_,name,ipv);

        //建立连接时,指定必要的参数
        connection_->setConnectStatus(true);
        connection_->SetName(name);
        //connection_ = make_shared<TcpConnection>(loop_, name, socket_);
        connection_->setMessageCallback(std::bind(&TcpClient::onMessage,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
        connection_->setConnectCloseCallback(std::bind(&TcpClient::onConnectClose,this,std::placeholders::_1));
        runConnectCallback(TcpClient::OnConnectSuccess);
    }
    else
    {
        if (::uv_is_active((uv_handle_t*)socket_))
        {
            ::uv_read_stop((uv_stream_t*)socket_);
        }
        if (::uv_is_closing((uv_handle_t*)socket_) == 0)
        {
            socket_->data = static_cast<void*>(that_);
            ::uv_close((uv_handle_t*)socket_,
                [](uv_handle_t* handle)
            {
                that* that_ = static_cast<that*>(handle->data);
                if (!that_->getWillGoner()) {
                    auto client = static_cast<TcpClient*>(that_->body());
                    client->afterConnectFail();
                    that_->rtnWillGoner();
                }
            });
        }
    }
}
void TcpClient::onConnectClose(string& name)
{
    if (connection_)
    {
        connection_->close(std::bind(&TcpClient::onClose,this,std::placeholders::_1));
    }

}
void TcpClient::onMessage(shared_ptr<TcpConnection> connection,const char* buf,ssize_t size)
{
    if(onMessageCallback_)
        onMessageCallback_(buf,size);
}

void uv::TcpClient::close(std::function<void(uv::TcpClient*)> callback)
{
    if (connection_)
    {
        that* that__ = that_;
        connection_->close([that__, callback](std::string& name)
            {
                if (that__->getWillGoner()) {
                    if (that__->onGoneCallback()) {
                        delete that__;
                    }
                }else{
                    //onClose(name);
                    if (callback) {
                        auto client = static_cast<TcpClient*>(that__->body());
                        callback(client);
                    }
                    that__->rtnWillGoner();
                }
            });

    }
    else if(callback)
    {
        callback(this);
    }
}

void uv::TcpClient::afterConnectFail()
{
    runConnectCallback(TcpClient::OnConnnectFail);
}

void uv::TcpClient::write(const char* buf, unsigned int size, AfterWriteCallback callback)
{
    if (connection_)
    {
        connection_->write(buf, size, callback);
    }
    else if(callback)
    {
        uv::LogWriter::Instance()->warn("try write a disconnect connection.");
        WriteInfo info = { WriteInfo::Disconnected,const_cast<char*>(buf),size };
        callback(info);
    }

}

void uv::TcpClient::writeInLoop(const char * buf, unsigned int size, AfterWriteCallback callback)
{
    if (connection_)
    {
        connection_->writeInLoop(buf, size, callback);
    }
    else if(callback)
    {
        uv::LogWriter::Instance()->warn("try write a disconnect connection.");
        WriteInfo info = { WriteInfo::Disconnected,const_cast<char*>(buf),size };
        callback(info);
    }
}

void uv::TcpClient::setConnectStatusCallback(ConnectStatusCallback callback)
{
    connectCallback_ = callback;
}

void uv::TcpClient::setMessageCallback(NewMessageCallback callback)
{
    onMessageCallback_ = callback;
}

EventLoop* uv::TcpClient::Loop()
{
    return loop_;
}

TcpConnectionPtr uv::TcpClient::getTcpConnectionPtr()
{
    return connection_;
}

PacketBufferPtr uv::TcpClient::getCurrentBuf()
{
    if (connection_)
        return connection_->getPacketBuffer();
    return nullptr;
}


void TcpClient::update()
{
    socket_ = new uv_tcp_t;
    ::uv_tcp_init(loop_->handle(), socket_);
    if (tcpNoDelay_)
        ::uv_tcp_nodelay(socket_, 1 );
}

void uv::TcpClient::runConnectCallback(TcpClient::ConnectStatus satus)
{
    if (connectCallback_)
        connectCallback_(satus);
}

void uv::TcpClient::onClose(std::string& name)
{
    //connection_ = nullptr;
    uv::LogWriter::Instance()->info("Close tcp client connection complete.");
    runConnectCallback(TcpClient::OnConnnectClose);
}
