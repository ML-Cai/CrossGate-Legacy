#include "assets/animation_decode_worker.h"
#include <stdexcept>

namespace cgl {
bool AnimationDecodeWorker::submit(Task task,uint64_t bytes) {
    if(!task || !bytes || bytes>MaxBytes) throw std::invalid_argument("Invalid decode reservation");
    std::lock_guard lock(mutex_);
    if(reserved_) return false;
    if(!thread_.joinable()) {
        stopping_=false;
        thread_=std::thread([this]{run();});
    }
    cancelled_.store(false);
    reserved_=bytes;job_=std::move(task);wake_.notify_one();
    return true;
}
std::optional<AnimationDecodeWorker::Result> AnimationDecodeWorker::tryTake() {
    std::lock_guard lock(mutex_);
    if(!result_) return std::nullopt;
    auto result=std::move(result_);result_.reset();reserved_=0;
    return result;
}
uint64_t AnimationDecodeWorker::reservedBytes() const {
    std::lock_guard lock(mutex_);return reserved_;
}
void AnimationDecodeWorker::requestCancel() noexcept {
    std::lock_guard lock(mutex_);
    cancelled_.store(true);job_={};
    if(!running_) {result_.reset();reserved_=0;}
}
bool AnimationDecodeWorker::collectCancelled() noexcept {
    std::lock_guard lock(mutex_);
    if(!cancelled_.load() || running_) return false;
    result_.reset();reserved_=0;return true;
}
void AnimationDecodeWorker::cancel() noexcept {
    std::unique_lock lock(mutex_);
    cancelled_.store(true);job_={};
    done_.wait(lock,[this]{return !running_;});
    result_.reset();reserved_=0;
}
void AnimationDecodeWorker::stop() noexcept {
    cancel();
    {
        std::lock_guard lock(mutex_);stopping_=true;wake_.notify_one();
    }
    if(thread_.joinable()) thread_.join();
}
void AnimationDecodeWorker::run() {
    std::unique_lock lock(mutex_);
    for(;;) {
        wake_.wait(lock,[this]{return stopping_ || bool(job_);});
        if(stopping_) return;
        auto task=std::move(job_);job_={};running_=true;
        const auto limit=reserved_;
        lock.unlock();
        Result output;
        try {
            output.pixels=task(cancelled_);
            if(output.pixels.rgba.capacity()>limit) throw std::runtime_error("Decoded result exceeds reservation");
        } catch(...) {output.pixels={};output.error=std::current_exception();}
        task={};
        lock.lock();
        if(!cancelled_.load()) result_=std::move(output);
        output={};running_=false;done_.notify_all();
    }
}
}
