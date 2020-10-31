/*
   Copyright © 2017-2019, orcaer@yeah.net  All rights reserved.

   Author: orcaer@yeah.net

   Last modified: 2019-6-17

   Description: https://github.com/wlgq2/uv-cpp
*/

#ifndef   UV_EVENT_LOOP_HPP
#define   UV_EVENT_LOOP_HPP

#include <uv.h>
#include <thread>
#include <atomic>
#include <functional>
#include "ResurPoint.h"

namespace uv
{
using DefaultCallback = std::function<void()>;
class EventLoop;
using EventLoopPtr = std::shared_ptr<uv::EventLoop>;

class Async;
class EventLoop
{
public:
    enum Mode
    {
        Default,
        New
    };
    enum Status
    {
        NotRun,
        Runed,
        Stop
    };
    EventLoop(Mode mode);
    EventLoop();
    ~EventLoop();

    static EventLoop* DefaultLoop();
    static EventLoopPtr DefaultLoopPtr();

    HANDLE runInNewThread(const DefaultCallback onExit = nullptr);
    void close(bool bWaitForExit = true);
    int run();
    int runNoWait();
    int stop();
    bool isStoped();
    Status getStatus();
    std::thread::id getLoopThreadId() { return loopThreadId_; }
    bool isRunInLoopThread();
    bool isRunInDefaultLoop();
    //Calling onCall function from this loop thread, and Call onClose function When call close();
    //When onCall is "Blocked function", it must exit when call close.
    void runInThisLoop(const DefaultCallback onCall, const DefaultCallback onClose = nullptr);
    uv_loop_t* handle();

    static const char* GetErrorMessage(int status);
    DefaultCallback getExitRunThreadCallback();
    DefaultCallback getCloseRunThreadCallback();

private:

    std::thread::id loopThreadId_;
    uv_loop_t* loop_;
    Async* async_;
    std::atomic<Status> status_;
    HANDLE loopThreadHandle_;
    DefaultCallback onExitRunThreadCallback;
    DefaultCallback onCloseRunThreadCallback;
    bool runInDefaultLoop;
};
}
#endif

