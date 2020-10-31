#pragma once
#include <mutex>

typedef int (*that_gone_cb)(void* hpara, void*lpara);

class that
{
private:
	void* body_ = nullptr;
	bool willGoner_ = false;
    std::recursive_mutex mutCodeGuarder_;//code guarder for destructor and callback function.
    void* goneCallbackPara_ = nullptr;
    that_gone_cb goneCallback_ = nullptr;
public:
    that(void* body = nullptr) {
        if (body == nullptr) {
            body_ = this;
        }
        else {
            body_ = body;
        }
    }
	~that() {
    };
	void* body() {
        return body_;
    }
	void setWillGoner() {
        mutCodeGuarder_.lock();
        willGoner_ = true;
        mutCodeGuarder_.unlock();
    }
	bool getWillGoner() {
        if (willGoner_) {
            return true;
        }
        mutCodeGuarder_.lock();
        return willGoner_;
    }
    void rtnWillGoner() {
        mutCodeGuarder_.unlock();
    }
    void setGoneCallback(void* para, that_gone_cb cb) {
        goneCallbackPara_ = para;
        goneCallback_ = cb;
    }
    int onGoneCallback(void* p = nullptr) {//返回值,可以用于在回调函数后期处理
        if (goneCallback_) {
            return goneCallback_(goneCallbackPara_, p);
        }
        return 1;
    }
};