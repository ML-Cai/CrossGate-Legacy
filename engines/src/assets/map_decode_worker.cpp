#include "assets/map_decode_worker.h"
#include <stdexcept>

namespace cgl {
bool MapDecodeWorker::submit(Task task,uint64_t bytes) {
    if(!task || bytes>MaxBytes) throw std::invalid_argument("Invalid map decode reservation");
    std::lock_guard lock(mutex_);
    if(occupied_) return false;
    if(!thread_.joinable()) {stopping_=false;thread_=std::thread([this]{run();});}
    cancelled_.store(false);occupied_=true;reserved_=bytes;task_=std::move(task);wake_.notify_one();return true;
}
std::optional<MapDecodeWorker::Result> MapDecodeWorker::tryTake() {
    std::lock_guard lock(mutex_);
    if(!result_) return std::nullopt;
    auto result=std::move(result_);result_.reset();reserved_=0;occupied_=false;return result;
}
bool MapDecodeWorker::busy() const {std::lock_guard lock(mutex_);return occupied_;}
uint64_t MapDecodeWorker::reservedBytes() const {std::lock_guard lock(mutex_);return reserved_;}
void MapDecodeWorker::requestCancel() noexcept {
    std::lock_guard lock(mutex_);cancelled_.store(true);task_={};
    if(!running_) {result_.reset();reserved_=0;occupied_=false;}
}
bool MapDecodeWorker::collectCancelled() noexcept {
    std::lock_guard lock(mutex_);
    if(!cancelled_.load() || running_) return false;
    result_.reset();reserved_=0;occupied_=false;return true;
}
void MapDecodeWorker::cancel() noexcept {
    std::unique_lock lock(mutex_);cancelled_.store(true);task_={};
    done_.wait(lock,[this]{return !running_;});result_.reset();reserved_=0;occupied_=false;
}
void MapDecodeWorker::stop() noexcept {
    cancel();{std::lock_guard lock(mutex_);stopping_=true;wake_.notify_one();}
    if(thread_.joinable()) thread_.join();
}
void MapDecodeWorker::run() {
    std::unique_lock lock(mutex_);
    for(;;) {
        wake_.wait(lock,[this]{return stopping_ || bool(task_);});if(stopping_) return;
        auto task=std::move(task_);task_={};running_=true;lock.unlock();
        Result output;
        try {
            output=task(cancelled_);
            if(output.pixels.rgba.capacity()>reservedBytes()) throw std::runtime_error("Map pixels exceed reservation");
        } catch(...) {output={};output.error=std::current_exception();}
        task={};lock.lock();
        if(!cancelled_.load()) result_=std::move(output);
        output={};running_=false;done_.notify_all();
    }
}
}
