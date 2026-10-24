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
// One reservation covers queued, running and ready pixels. No unbounded job queue.
class AnimationDecodeWorker final {
public:
    static constexpr uint64_t MaxBytes=32ull*1024*1024;
    using Task=std::function<AnimationPixels(const std::atomic_bool&)>;
    struct Result { AnimationPixels pixels; std::exception_ptr error; };
    ~AnimationDecodeWorker() { stop(); }
    bool submit(Task task,uint64_t bytes);
    std::optional<Result> tryTake();
    uint64_t reservedBytes() const;
    // Cooperative cancellation waits for active CPU access to end; discards output.
    // Owner-thread nonblocking discard: reservation remains until CPU access ends.
    void requestCancel() noexcept;
    bool collectCancelled() noexcept;
    void cancel() noexcept;
    void stop() noexcept;
private:
    void run();
    mutable std::mutex mutex_;
    std::condition_variable wake_,done_;
    std::thread thread_;
    Task job_;
    std::optional<Result> result_;
    std::atomic_bool cancelled_{false};
    uint64_t reserved_=0;
    bool running_=false,stopping_=false;
};
}
