/*
   Copyright © 2017-2020, orcaer@yeah.net  All rights reserved.

   Author: orcaer@yeah.net
    
   Last modified: 2019-9-10
    
   Description: uv-cpp
*/

#include "include/Timer.hpp"

using namespace uv;

Timer::Timer(EventLoop * loop, uint64_t timeout, uint64_t repeat, TimerCallback callback)
    :that_(new that(this)),
    loop_(loop),
    started_(false),
    handle_(new uv_timer_t),
    timeout_(timeout),
    repeat_(repeat),
    callback_(callback)
{
    handle_->data = static_cast<void*>(that_);
    ::uv_timer_init(loop->handle(), handle_);
}

Timer::~Timer()
{
    that_->setWillGoner();
    close(nullptr);
}

void Timer::start()
{
    if (!started_)
    {
        started_ = true;
        ::uv_timer_start(handle_, Timer::process, timeout_, repeat_);
    }
}

void Timer::close(TimerCloseComplete callback)
{
    callback_ = nullptr;
    closeComplete_ = callback;
    if (uv_is_active((uv_handle_t*)handle_))
    {
        uv_timer_stop(handle_);
    }
    if (uv_is_closing((uv_handle_t*)handle_) == 0)
    {
        ::uv_close((uv_handle_t*)handle_,
            [](uv_handle_t* handle)
            {
                that* that_ = static_cast<that*>(handle->data);
                if (that_->getWillGoner()) {
                    delete that_;
                    delete handle;
                }
                else {
                    auto ptrTimer = static_cast<Timer*>(that_->body());
                    if (ptrTimer->closeComplete_) {
                        ptrTimer->closeComplete_(ptrTimer);
                    }
                    that_->rtnWillGoner();
                }
            });
        //投递一个通知,催促尽快处理
        PostQueuedCompletionStatus(loop_->handle()->iocp, 0, 0, NULL);
    }
    else
    {
        if (callback) {
            callback(this);
        }
    }
}


void Timer::setTimerRepeat(uint64_t ms)
{
    repeat_ = ms;
    ::uv_timer_set_repeat(handle_, ms);
}


void Timer::onTimeOut()
{
    if (callback_)
    {
        callback_(this);
    }
}

void Timer::process(uv_timer_t * handle)
{
    that* that_ = static_cast<that*>(handle->data);
    if (!that_->getWillGoner()) {
        auto ptrTimer = static_cast<Timer*>(that_->body());
        ptrTimer->onTimeOut();
        that_->rtnWillGoner();
    }
}


