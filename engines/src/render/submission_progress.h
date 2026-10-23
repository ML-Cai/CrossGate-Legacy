#pragma once
#include <algorithm>
#include <cstdint>
namespace cgl {
// Single graphics queue only. Publish a submission AFTER successful queue submit;
// acknowledge its slot serial only AFTER a successful wait on that slot's fence.
struct SubmissionProgress {
    uint64_t submitted=0,completed=0;
    uint64_t didSubmit() noexcept {return ++submitted;}
    void didComplete(uint64_t serial) noexcept {
        completed=std::max(completed,std::min(serial,submitted));
    }
};
}
