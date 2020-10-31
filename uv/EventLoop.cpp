/*
   Copyright © 2017-2020, orcaer@yeah.net  All rights reserved.

   Author: orcaer@yeah.net
    
   Last modified: 2019-6-17
    
   Description: https://github.com/wlgq2/uv-cpp
*/

#include "include/EventLoop.hpp"
#include "include/TcpConnection.hpp"
#include "include/Async.hpp"

using namespace uv;

EventLoop::EventLoop()
    :EventLoop(EventLoop::Mode::New)
{
}

EventLoop::EventLoop(EventLoop::Mode mode)
    :loop_(nullptr),
    async_(nullptr),
    status_(NotRun),
    loopThreadHandle_(nullptr),
    onExitRunThreadCallback(nullptr),
    onCloseRunThreadCallback(nullptr)
{
    if (mode == EventLoop::Mode::New)
    {
        loop_ = new uv_loop_t();
        ::uv_loop_init(loop_);
        runInDefaultLoop = false;
    }
    else
    {
        loop_ = uv_default_loop();
        runInDefaultLoop = true;
    }
    async_ = new Async(this);
}

EventLoop::~EventLoop()
{
    if (loop_ && loop_ != uv_default_loop())
    {
        uv_loop_close(loop_);
        delete async_;
        delete loop_;
    }
}

EventLoop* uv::EventLoop::DefaultLoop()
{
    static EventLoop defaultLoop(EventLoop::Mode::Default);
    return &defaultLoop;
}
EventLoopPtr uv::EventLoop::DefaultLoopPtr()
{
    static EventLoopPtr defaultLoop = std::make_shared<EventLoop>(EventLoop::Mode::Default);
    return defaultLoop;
}

uv_loop_t* EventLoop::handle()
{
    return loop_;
}

int EventLoop::run()
{
    if (status_ == Status::NotRun
        || status_ == Status::Stop) // 解决停止后再启动,不能启动的BUG
    {
        async_->init();
    	loopThreadId_ = std::this_thread::get_id();
    	status_ = Status::Runed;
    	auto rst = ::uv_run(loop_, UV_RUN_DEFAULT);
    	status_ = Status::Stop;
    	return rst;
    }
    return -1;
}

void uv_cpp_run_thread(void* args)
{
    EventLoop* worker = (EventLoop*)args;
    worker->run();
    if (worker->getExitRunThreadCallback()) {
        worker->getExitRunThreadCallback()();
    }
}

HANDLE EventLoop::runInNewThread(const DefaultCallback onExit)
{
    onExitRunThreadCallback = onExit;
    loopThreadHandle_ = (HANDLE)_beginthread(uv_cpp_run_thread, 0, this);
    return loopThreadHandle_;
}
DefaultCallback EventLoop::getExitRunThreadCallback()
{
    return onExitRunThreadCallback;
}
DefaultCallback EventLoop::getCloseRunThreadCallback()
{
    return onCloseRunThreadCallback;
}

void EventLoop::close(bool bWaitForExit)
{
    if (getCloseRunThreadCallback()) {
        getCloseRunThreadCallback()();
    }
    if (loop_) {
        loop_->stop_flag = true;
        PostQueuedCompletionStatus(loop_->iocp, 0, 0, NULL);
        if (bWaitForExit) {
            if (loopThreadHandle_) {
                WaitForSingleObject(loopThreadHandle_, INFINITE);
            }
        }
    }
}

int uv::EventLoop::runNoWait()
{
    if (status_ == Status::NotRun)
    {
        async_->init();
    	loopThreadId_ = std::this_thread::get_id();
    	status_ = Status::Runed;
        auto rst = ::uv_run(loop_, UV_RUN_NOWAIT);
        status_ = Status::NotRun;
        return rst;
    }
    return -1;
}

int uv::EventLoop::stop()
{
    if (status_ == Status::Runed)
    {
        async_->close([](Async* ptr)
        {
            ::uv_stop(ptr->Loop()->handle());
        });
        return 0;
    }
    return -1;
}

bool EventLoop::isStoped()
{
    return status_ == Status::Stop;
}

EventLoop::Status EventLoop::getStatus()
{
    return status_;
}

bool EventLoop::isRunInDefaultLoop()
{
    return runInDefaultLoop;
}

bool EventLoop::isRunInLoopThread()
{
    if (status_ == Status::Runed)
    {
        return std::this_thread::get_id() == loopThreadId_;
    }
    //EventLoop未运行.
    return false;
}

void uv::EventLoop::runInThisLoop(const DefaultCallback onCall, const DefaultCallback onClose)
{
    if (nullptr == onCall)
        return;

    onCloseRunThreadCallback = onClose;
    if (isRunInLoopThread() || isStoped())
    {
        onCall();
        return;
    }
    async_->runInThisLoop(onCall);
}

const char* EventLoop::GetErrorMessage(int status)
{
    if (WriteInfo::Disconnected == status)
    {
        static char info[] = "the connection is disconnected";
        return info;
    }
    return uv_strerror(status);
}