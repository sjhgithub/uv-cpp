/*
Copyright © 2017-2020, orcaer@yeah.net  All rights reserved.

Author: orcaer@yeah.net

Last modified: 2019-12-31

Description: https://github.com/wlgq2/uv-cpp
*/

#include "include/TcpConnection.hpp"
#include "include/TcpServer.hpp"
#include "include/Async.hpp"
#include "include/LogWriter.hpp"
#include "include/GlobalConfig.hpp"

using namespace std;
using namespace std::chrono;
using namespace uv;

struct WriteReq
{
    uv_write_t req;
    uv_buf_t buf;
    AfterWriteCallback callback;
};

struct WriteArgs
{
    WriteArgs(shared_ptr<TcpConnection> conn = nullptr, const char* buf = nullptr, ssize_t size = 0, AfterWriteCallback callback = nullptr)
        :connection(conn),
        buf(buf),
        size(size),
        callback(callback)
    {

    }
    shared_ptr<TcpConnection> connection;
    const char* buf;
    ssize_t size;
    AfterWriteCallback callback;
};

TcpConnection:: ~TcpConnection()
{
    that_->setWillGoner();
    close(nullptr);
}

TcpConnection::TcpConnection(EventLoop* loop, std::string& name, uv_tcp_t* client, bool isConnected, bool bAutoStartReading)
    :that_(new that(this)),
    name_(name),
    connected_(isConnected),
    loop_(loop),
    handle_(client),
    buffer_(nullptr),
    userdata(nullptr),
    parent(nullptr),
    m_bAutoStartReading(bAutoStartReading),
    onMessageCallback_(nullptr),
    onConnectCloseCallback_(nullptr),
    closeCompleteCallback_(nullptr)
{
    handle_->data = static_cast<void*>(that_);
    if (m_bAutoStartReading && connected_) {
        startReading();
    }
    if (GlobalConfig::BufferModeStatus == GlobalConfig::ListBuffer)
    {
        buffer_ = std::make_shared<ListBuffer>();
    }
    else if(GlobalConfig::BufferModeStatus == GlobalConfig::CycleBuffer)
    {
        buffer_ = std::make_shared<CycleBuffer>();
    }
    goneCallbackPara_ = new GONECALLBACKPARA;
}

void TcpConnection::onMessage(const char* buf, ssize_t size)
{
    if (onMessageCallback_)
        onMessageCallback_(shared_from_this(), buf, size);
}

void TcpConnection::onSocketClose()
{
    if (onConnectCloseCallback_)
        onConnectCloseCallback_(name_);
}

void TcpConnection::close(std::function<void(std::string&)> callback)
{
    onMessageCallback_ = nullptr;
    onConnectCloseCallback_ = nullptr;
    //closeCompleteCallback_ = nullptr;

    closeCompleteCallback_ = callback;
    if (::uv_is_active((uv_handle_t*)handle_))
    {
        ::uv_read_stop((uv_stream_t*)handle_);
    }
    if (::uv_is_closing((uv_handle_t*)handle_) == 0)
    {
        if (callback) {
            goneCallbackPara_->callback = callback;
            goneCallbackPara_->name = name_;
            that_->setGoneCallback((void*)goneCallbackPara_,
                [](void* hpara, void* lpara) -> int {
                    GONECALLBACKPARA* cbPara = (GONECALLBACKPARA*)hpara;
                    if (cbPara && cbPara->callback) {
                        cbPara->callback(cbPara->name);
                    }
                    return 1;
                });
        }
        //libuv 在loop轮询中会检测关闭句柄，delete会导致程序异常退出。
        ::uv_close((uv_handle_t*)handle_,
            [](uv_handle_t* handle)
            {
                that* that_ = static_cast<that*>(handle->data);
                if (that_->getWillGoner()) {
                    if (that_->onGoneCallback()) {
                        delete that_;
                    }
                }
                else {
                    auto connection = static_cast<TcpConnection*>(that_->body());
                    connection->CloseComplete();
                    that_->rtnWillGoner();
                }
            });
    }
    else
    {
        CloseComplete();
    }
}

void TcpConnection::startReading()
{
    ::uv_read_start((uv_stream_t*)handle_,
        [](uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
        {
            that* that_ = static_cast<that*>(handle->data);
            if (!that_->getWillGoner()) {
                auto conn = static_cast<TcpConnection*>(that_->body());
                buf->base = conn->resizeData(suggested_size);
#if _MSC_VER
                buf->len = (ULONG)suggested_size;
#else
                buf->len = suggested_size;
#endif
                that_->rtnWillGoner();
            }
        },
        &TcpConnection::onMesageReceive);
}

int TcpConnection::write(const char* buf, ssize_t size, AfterWriteCallback callback)
{
    int rst;
    if (connected_)
    {
        WriteReq* req = new WriteReq;
        req->buf = uv_buf_init(const_cast<char*>(buf), static_cast<unsigned int>(size));
        req->callback = callback;
        req->req.data = that_;
        rst = ::uv_write((uv_write_t*)req, (uv_stream_t*)handle_, &req->buf, 1,
            [](uv_write_t *req, int status)
        {
            WriteReq* wr = (WriteReq*)req;
            that* that_ = (that*)wr->req.data;
            if (!that_->getWillGoner()) {
                if (nullptr != wr->callback)
                {
                    struct WriteInfo info;
                    info.buf = const_cast<char*>(wr->buf.base);
                    info.size = wr->buf.len;
                    info.status = status;
                    wr->callback(info);
                }
                that_->rtnWillGoner();
            }
            delete wr;
        });
        if (0 != rst)
        {
            uv::LogWriter::Instance()->error(std::string("write data error:"+std::to_string(rst)));
            if (nullptr != callback)
            {
                struct WriteInfo info = { rst,const_cast<char*>(buf),static_cast<unsigned long>(size) };
                callback(info);
            }
            delete req;
        }
    }
    else
    {
        rst = -1;
        if (nullptr != callback)
        {
            struct WriteInfo info = { WriteInfo::Disconnected,const_cast<char*>(buf),static_cast<unsigned long>(size) };
            callback(info);
        }
    }
    return rst;
}

void TcpConnection::writeInLoop(const char* buf, ssize_t size, AfterWriteCallback callback)
{
    //std::weak_ptr<uv::TcpConnection> conn = shared_from_this();
    that* that__ = that_;
    loop_->runInThisLoop(
        [that__,buf,size, callback]()
    {
        //std::shared_ptr<uv::TcpConnection> ptr = conn.lock();
        if (!that__->getWillGoner()) {
            uv::TcpConnection* ptr = static_cast<TcpConnection*>(that__->body());
            if (ptr != nullptr)
            {
                ptr->write(buf, size, callback);
            }
            else
            {
                struct WriteInfo info = { WriteInfo::Disconnected,const_cast<char*>(buf),static_cast<unsigned long>(size) };
                callback(info);
            }
            that__->rtnWillGoner();
        }
    });
}


void TcpConnection::setWrapper(ConnectionWrapperPtr wrapper)
{
    wrapper_ = wrapper;
}

std::shared_ptr<ConnectionWrapper> TcpConnection::getWrapper()
{
    return wrapper_.lock();
}

void  TcpConnection::onMesageReceive(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf)
{
    that* that_ = static_cast<that*>(client->data);
    if (that_->getWillGoner()) {
        int iTrap = 1;
    }
    else {
        auto connection = static_cast<TcpConnection*>(that_->body());
	    if (connection->dbgRawMesageReceive_) {
		    if (!connection->fpDbgLogger_) {
                connection->fpDbgLogger_ = std::fopen("onMesageReceive.dat", "wb");//不指定b标志,则遇到0D,会加上0A字符
		    }
	    }
        if (nread > 0)
        {
		    if (connection->dbgRawMesageReceive_ && connection->fpDbgLogger_) {
			    std::fwrite(buf->base, 1, nread, connection->fpDbgLogger_);
		    }
            connection->onMessage(buf->base, nread);
        }
        else if (nread < 0)
        {
            //设置状态语句已移动到 CloseComplete()
            //connection->setConnectStatus(false);
            std::string serr = uv_err_name((int)nread);
            uv::LogWriter::Instance()->error(serr);
            if (connection->dbgRawMesageReceive_ && connection->fpDbgLogger_) {
                std::fwrite(serr.c_str(), 1, serr.length(), connection->fpDbgLogger_);
            }

            if (nread != UV_EOF)
            {
                connection->onSocketClose();
            }
            else {
                uv_shutdown_t* sreq = new uv_shutdown_t;
                sreq->data = static_cast<void*>(that_);
                ::uv_shutdown(sreq, (uv_stream_t*)client,
                    [](uv_shutdown_t* req, int status)
                    {
                        that* that_ = static_cast<that*>(req->data);
                        if (!that_->getWillGoner()) {
                            auto connection = static_cast<TcpConnection*>(that_->body());
                            connection->onSocketClose();
                            that_->rtnWillGoner();
                        }
                        delete req;
                    });
            }
        }
        else
        {
            /* Everything OK, but nothing read. */
        }
        that_->rtnWillGoner();
    }
}

void uv::TcpConnection::setMessageCallback(OnMessageCallback callback)
{
    onMessageCallback_ = callback;
}

void uv::TcpConnection::setConnectCloseCallback(OnCloseCallback callback)
{
    onConnectCloseCallback_ = callback;
}

void uv::TcpConnection::CloseComplete()
{
    setConnectStatus(false);
    if (closeCompleteCallback_)
    {
        closeCompleteCallback_(name_);
    }
}

void uv::TcpConnection::setConnectStatus(bool status)
{
    connected_ = status;
    if (m_bAutoStartReading && connected_) {
        startReading();
    }
}

bool uv::TcpConnection::isConnected()
{
    return connected_;
}

const std::string& uv::TcpConnection::Name()
{
    return name_;
}

void uv::TcpConnection::SetName(std::string& name)
{
    name_ = name;
}

char* uv::TcpConnection::resizeData(size_t size)
{
    data_.resize(size);
    return const_cast<char*>(data_.c_str());
}

PacketBufferPtr uv::TcpConnection::getPacketBuffer()
{
    return buffer_;
}
