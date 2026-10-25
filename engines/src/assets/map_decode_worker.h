#pragma once
#include "assets/map_builder.h"
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

namespace cgl {
// Owner-thread submit/take/cancel. One slot includes running work and completed
// output, so a slow consumer never accumulates pages. No GPU calls or job queue.
class MapDecodeWorker final {
public:
    static constexpr uint64_t MaxBytes=32ull*1024*1024;
    struct Result {
        std::unique_ptr<MapBuilder> plan;
        MapPagePixels pixels{};
        std::exception_ptr error;
    };
    using Task=std::function<Result(const std::atomic_bool&)>;
    ~MapDecodeWorker() {stop();}
    bool submit(Task task,uint64_t bytes);
    std::optional<Result> tryTake();
    bool busy() const;
    uint64_t reservedBytes() const;
    // Nonblocking discard keeps the slot/reservation until CPU access ends.
    void requestCancel() noexcept;
    bool collectCancelled() noexcept;
    void cancel() noexcept;
    void stop() noexcept;
private:
    void run();
    mutable std::mutex mutex_;
    std::condition_variable wake_,done_;
    std::thread thread_;
    Task task_;
    std::optional<Result> result_;
    std::atomic_bool cancelled_{false};
    uint64_t reserved_=0;
    bool occupied_=false,running_=false,stopping_=false;
};
}
