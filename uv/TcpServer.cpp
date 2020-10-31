/*
   Copyright © 2017-2020, orcaer@yeah.net  All rights reserved.

   Author: orcaer@yeah.net

   Last modified: 2019-12-31

   Description: https://github.com/wlgq2/uv-cpp
*/


#include <functional>
#include <memory>
#include <string>

#include "include/TcpServer.hpp"
#include "include/LogWriter.hpp"

using namespace std;
using namespace uv;


void uv::TcpServer::SetBufferMode(uv::GlobalConfig::BufferMode mode)
{
    uv::GlobalConfig::BufferModeStatus = mode;
}

TcpServer::TcpServer(EventLoop* loop, bool tcpNoDelay, bool bAutoStartReading)
    :that_(new that(this)),
    loop_(loop),
    tcpNoDelay_(tcpNoDelay),
    accetper_(nullptr),
    onMessageCallback_(nullptr),
    onNewConnectCallback_(nullptr),
    onConnectCloseCallback_(nullptr),
    timerWheel_(loop),
    m_bAutoStartReading(bAutoStartReading)
{
}
TcpServer:: ~TcpServer()
{
    that_->setWillGoner();
    //拒绝接收任何外界接入
    accetper_->setNewConnectinonCallback(nullptr);
    //简单的对connnections_进行clear或者swap清空操作,并不能触发TcpConnectionPtr的析构,因为有可能在其他地方也有引用
    //所以为了完美的释放内存,需要将所有连接复制一份,并调用close进行关闭,然后在回调中清理复制的map
    muxConnections_.lock();
    //复制一份连接map
    typedef struct goneCbPara
    {
        std::map<std::string, TcpConnectionPtr> connnections_;
        OnConnectionStatusCallback onConnectCloseCallback_;
    };
    goneCbPara* cbPara = new goneCbPara;
    cbPara->connnections_ = connnections_;
    cbPara->onConnectCloseCallback_ = onConnectCloseCallback_;
    that_->setGoneCallback((void*)cbPara,
        [](void* hpara, void* lpara) -> int {
            goneCbPara* cbPara = (goneCbPara*)hpara;
            int ret = 0;
            if (cbPara && lpara) {//lpara 指向连接名
                if (cbPara->connnections_.size() > 0) {
                    std::string name = (char*)lpara;
                    auto it = cbPara->connnections_.find(name);
                    if (it != cbPara->connnections_.end()) {
                        if (cbPara->onConnectCloseCallback_)
                        {
                            cbPara->onConnectCloseCallback_(it->second);
                        }
                        cbPara->connnections_.erase(name);
                    }
                }
                if (cbPara->connnections_.size() == 0) {
                    ret = 1;
                    delete cbPara;
                }
            }
            else {
                ret = 1;
                if (cbPara) {
                    delete cbPara;
                }
            }
            return ret;
        });
    //然后再进行关闭
    for (map<std::string, TcpConnectionPtr>::iterator it = connnections_.begin(); it != connnections_.end(); it++) {
        closeConnectionPtr(it->second);
    }
    //清理掉
    connnections_.clear();
    muxConnections_.unlock();
}

void TcpServer::setTimeout(unsigned int seconds)
{
    timerWheel_.setTimeout(seconds);
}

void uv::TcpServer::onAccept(EventLoop * loop, uv_tcp_t* client)
{
    string key;
    SocketAddr::AddrToStr(client, key, ipv_);

    uv::LogWriter::Instance()->debug("new connect  " + key);
    shared_ptr<TcpConnection> connection(new TcpConnection(loop, key, client, true, m_bAutoStartReading));
    if (connection)
    {
        connection->parent = this;
        connection->setMessageCallback(std::bind(&TcpServer::onMessage, this, placeholders::_1, placeholders::_2, placeholders::_3));
        connection->setConnectCloseCallback(std::bind(&TcpServer::closeConnection, this, placeholders::_1));
        addConnnection(key, connection);
        if (timerWheel_.getTimeout() > 0)
        {
            auto wrapper = std::make_shared<ConnectionWrapper>(connection);
            connection->setWrapper(wrapper);
            timerWheel_.insert(wrapper);
        }
        if (onNewConnectCallback_)
            onNewConnectCallback_(connection);
    }
    else
    {
        uv::LogWriter::Instance()->error("create connection fail. :" + key);
    }
}

int TcpServer::bindAndListen(SocketAddr& addr)
{
    ipv_ = addr.Ipv();
    accetper_ = std::make_shared<TcpAccepter>(loop_, tcpNoDelay_);
    auto rst = accetper_->bind(addr);
    if (0 != rst)
    {
        return rst;
    }
    accetper_->setNewConnectinonCallback(std::bind(&TcpServer::onAccept, this, std::placeholders::_1, std::placeholders::_2));
    timerWheel_.start();
    return accetper_->listen();
}

void TcpServer::close(DefaultCallback callback)
{
    if (accetper_)
        accetper_->close([this, callback]()
    {
        for (auto& connection : connnections_)
        {
            connection.second->onSocketClose();
        }
        callback();
    });
}

void TcpServer::addConnnection(std::string& name,TcpConnectionPtr connection)
{
    muxConnections_.lock();
    connnections_.insert(pair<string,shared_ptr<TcpConnection>>(std::move(name),connection));
    muxConnections_.unlock();
}

void TcpServer::removeConnnection(string& name)
{
    muxConnections_.lock();
    connnections_.erase(name);
    muxConnections_.unlock();
}

shared_ptr<TcpConnection> TcpServer::getConnnection(const string& name)
{
    std::unique_lock<std::mutex> lockConn(muxConnections_);
    auto rst = connnections_.find(name);
    if(rst == connnections_.end()) {
        return nullptr;
    }
    return rst->second;
}

std::map<std::string, TcpConnectionPtr>& TcpServer::getConnnections()
{
    return connnections_;
}

void TcpServer::closeConnection(const string& name)
{
    auto connection = getConnnection(name);
    if (nullptr != connection)
    {
        closeConnectionPtr(connection);
    }
}

void TcpServer::closeConnectionPtr(TcpConnectionPtr connection)
{
	connection->setMessageCallback(nullptr);
	connection->setConnectCloseCallback(nullptr);
    that* that__ = that_;
	connection->close([that__](std::string& name)
		{
            if (that__->getWillGoner()) {
                if (that__->onGoneCallback((void*)name.c_str())) {
                    delete that__;
                }
            }
            else {
                TcpServer* server = static_cast<TcpServer*>(that__->body());
			    auto connection = server->getConnnection(name);
			    if (nullptr != connection)
			    {
				    if (server->onConnectCloseCallback_)
				    {
                        server->onConnectCloseCallback_(connection);
				    }
                    server->removeConnnection(name);
			    }
                that__->rtnWillGoner();
            }
		});
}

void TcpServer::StartReading(TcpConnectionPtr connection)
{
    connection->startReading();
}
void TcpServer::StartReading(std::string& name)
{
    auto connection = getConnnection(name);
    if (nullptr != connection)
    {
        StartReading(connection);
    }
}

void TcpServer::onMessage(TcpConnectionPtr connection,const char* buf,ssize_t size)
{
    if(onMessageCallback_)
        onMessageCallback_(connection,buf,size);
    if (timerWheel_.getTimeout() > 0)
    {
        timerWheel_.insert(connection->getWrapper());
    }
}


void TcpServer::setMessageCallback(OnMessageCallback callback)
{
    onMessageCallback_ = callback;
}


void TcpServer::write(shared_ptr<TcpConnection> connection,const char* buf,unsigned int size, AfterWriteCallback callback)
{
    if(nullptr != connection)
    {
        connection->write(buf,size, callback);
        if (timerWheel_.getTimeout() > 0)
        {
            timerWheel_.insert(connection->getWrapper());
        }
    }
    else if (callback)
    {
        WriteInfo info = { WriteInfo::Disconnected,const_cast<char*>(buf),size };
        callback(info);
    }
}

void TcpServer::write(string& name,const char* buf,unsigned int size,AfterWriteCallback callback)
{
    auto connection = getConnnection(name);
    write(connection, buf, size, callback);
}

void TcpServer::writeInLoop(shared_ptr<TcpConnection> connection,const char* buf,unsigned int size,AfterWriteCallback callback)
{
    if(nullptr != connection)
    {
        connection->writeInLoop(buf,size,callback);
        if (timerWheel_.getTimeout() > 0)
        {
            timerWheel_.insert(connection->getWrapper());
        }
    }
    else if (callback)
    {
        uv::LogWriter::Instance()->warn("try write a disconnect connection.");
        WriteInfo info = { WriteInfo::Disconnected,const_cast<char*>(buf),size };
        callback(info);
    }
}

void TcpServer::writeInLoop(string& name,const char* buf,unsigned int size,AfterWriteCallback callback)
{
    auto connection = getConnnection(name);
    writeInLoop(connection, buf, size, callback);
}

void TcpServer::setNewConnectCallback(OnConnectionStatusCallback callback)
{
    onNewConnectCallback_ = callback;
}

void  TcpServer::setConnectCloseCallback(OnConnectionStatusCallback callback)
{
    onConnectCloseCallback_ = callback;
}
