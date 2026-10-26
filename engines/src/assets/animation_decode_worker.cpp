#include "assets/animation_decode_worker.h"
#include <stdexcept>
#include <chrono>

namespace cgl {
bool AnimationDecodeWorker::submit(Task task,uint64_t bytes) {
    if(!task || !bytes || bytes>MaxBytes) throw std::invalid_argument("Invalid decode reservation");
    return submitTask(std::move(task),bytes);
}
bool AnimationDecodeWorker::submitPlanned(Planner planner,Decode decode,uint64_t availableBytes,uint64_t clipLimit) {
    if(!planner || !decode || !clipLimit || clipLimit>AnimationBuilder::MaxBytes) throw std::invalid_argument("Invalid planned decode");
    return submitTask([this,plan=std::move(planner),decode=std::move(decode),availableBytes,clipLimit](const auto& cancelled) {
        const uint64_t required=plan(cancelled);
        {
            std::lock_guard lock(mutex_);
            if(cancelled.load()) throw std::runtime_error("Animation planning cancelled");
            if(!required || required>clipLimit) throw std::runtime_error("Animation exceeds progressive clip memory limit");
            if(required>availableBytes) throw AnimationBudgetExceeded(required);
            reserved_=required;
        }
        return decode(required,cancelled);
    },0);
}
bool AnimationDecodeWorker::submitTask(Task task,uint64_t bytes) {
    std::lock_guard lock(mutex_);
    if(occupied_) return false;
    if(!thread_.joinable()) {
        stopping_=false;
        thread_=std::thread([this]{run();});
    }
    cancelled_.store(false);
    occupied_=true;reserved_=bytes;job_=std::move(task);wake_.notify_one();
    return true;
}
std::optional<AnimationDecodeWorker::Result> AnimationDecodeWorker::tryTake() {
    std::lock_guard lock(mutex_);
    if(!result_) return std::nullopt;
    auto result=std::move(result_);result_.reset();reserved_=0;occupied_=false;
    return result;
}
bool AnimationDecodeWorker::busy() const {
    std::lock_guard lock(mutex_);return occupied_;
}
uint64_t AnimationDecodeWorker::reservedBytes() const {
    std::lock_guard lock(mutex_);return reserved_;
}
void AnimationDecodeWorker::requestCancel() noexcept {
    std::lock_guard lock(mutex_);
    cancelled_.store(true);job_={};
    if(!running_) {result_.reset();reserved_=0;occupied_=false;}
}
bool AnimationDecodeWorker::collectCancelled() noexcept {
    std::lock_guard lock(mutex_);
    if(!cancelled_.load() || running_) return false;
    result_.reset();reserved_=0;occupied_=false;return true;
}
void AnimationDecodeWorker::cancel() noexcept {
    std::unique_lock lock(mutex_);
    cancelled_.store(true);job_={};
    done_.wait(lock,[this]{return !running_;});
    result_.reset();reserved_=0;occupied_=false;
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
        const auto start=std::chrono::steady_clock::now();
        lock.unlock();
        Result output;
        try {
            output.pixels=task(cancelled_);
            if(output.pixels.rgba.capacity()>reservedBytes()) throw std::runtime_error("Decoded result exceeds reservation");
        } catch(...) {output.pixels={};output.error=std::current_exception();}
        output.cpuMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        task={};
        lock.lock();
        if(!cancelled_.load()) result_=std::move(output);
        output={};running_=false;done_.notify_all();
    }
}
}
