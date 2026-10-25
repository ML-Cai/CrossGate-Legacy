#pragma once
#include <cmath>
#include <cstdint>
#include <optional>
#include <limits>
namespace cgl::graphics::vulkan {
struct TimestampClock {
    uint32_t validBits=0;double periodNs=0;
    bool supported() const noexcept {return validBits>0 && validBits<=64 && std::isfinite(periodNs) && periodNs>0;}
    std::optional<double> milliseconds(uint64_t first,uint64_t last) const noexcept {
        if(!supported()) return std::nullopt;
        const uint64_t mask=validBits==64?std::numeric_limits<uint64_t>::max():(uint64_t(1)<<validBits)-1;
        // Unsigned subtraction also handles the 64-bit counter wrapping once.
        const auto ms=double((last-first)&mask)*periodNs/1000000.0;
        return std::isfinite(ms)?std::optional<double>(ms):std::nullopt;
    }
};
}
