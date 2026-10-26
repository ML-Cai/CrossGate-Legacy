#pragma once
#include "assets/animation_builder.h"
#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>

namespace cgl {
// CPU-only single-slot service. Submit/take/cancel/stop belong to one owner thread.
// One occupied slot covers planning, decoding and ready output. Planned RGBA
// reservation starts after metadata admission; no unbounded job queue.
class AnimationDecodeWorker final {
public:
    static constexpr uint64_t MaxBytes=32ull*1024*1024;
    using Task=std::function<AnimationPixels(const std::atomic_bool&)>;
    using Planner=std::function<uint64_t(const std::atomic_bool&)>;
    using Decode=std::function<AnimationPixels(uint64_t,const std::atomic_bool&)>;
    struct Result { AnimationPixels pixels; std::exception_ptr error;double cpuMs=0; };
    ~AnimationDecodeWorker() { stop(); }
    bool submit(Task task,uint64_t bytes);
    // Planning occupies the slot but reserves no RGBA. Reserve exact bytes before
    // decode; availableBytes is an owner-thread admission snapshot, not mutable.
    // The default 32 MiB streaming limit stays unchanged. Scene admission can
    // explicitly allow up to AnimationBuilder::MaxBytes in this same single slot.
    bool submitPlanned(Planner planner,Decode decode,uint64_t availableBytes,uint64_t clipLimit=MaxBytes);
    bool busy() const;
    std::optional<Result> tryTake();
    uint64_t reservedBytes() const;
    // Owner-thread nonblocking discard: reservation remains until CPU access ends.
    void requestCancel() noexcept;
    bool collectCancelled() noexcept;
    // Blocking lifetime boundary, unlike requestCancel.
    void cancel() noexcept;
    void stop() noexcept;
private:
    bool submitTask(Task task,uint64_t bytes);
    void run();
    mutable std::mutex mutex_;
    std::condition_variable wake_,done_;
    std::thread thread_;
    Task job_;
    std::optional<Result> result_;
    std::atomic_bool cancelled_{false};
    uint64_t reserved_=0;
    bool occupied_=false,running_=false,stopping_=false;
};
}
