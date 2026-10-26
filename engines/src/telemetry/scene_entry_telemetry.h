#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ostream>
namespace cgl {
enum class EntryStage {Plan,MapFinalize,CharacterClips,Catalog,RemoteWorld,Count};
struct EntryTiming {uint64_t calls=0;double totalMs=0,maxMs=0;};
// Owner-thread CPU wall time across scene visits. Fixed storage, no resource refs.
struct SceneEntryTimings {
    std::array<EntryTiming,size_t(EntryStage::Count)> stages{};
    bool add(EntryStage stage,double ms) noexcept {
        if(size_t(stage)>=stages.size() || !std::isfinite(ms) || ms<0) return false;
        auto& s=stages[size_t(stage)];++s.calls;s.totalMs+=ms;s.maxMs=std::max(s.maxMs,ms);return true;
    }
};
// Includes failed/cancelled calls; disabling telemetry avoids reading the clock.
class EntryTimer {
public:
    EntryTimer(SceneEntryTimings* timings,EntryStage stage) noexcept
        :timings_(timings),stage_(stage),start_(timings?Clock::now():Clock::time_point{}) {}
    ~EntryTimer() {if(timings_) timings_->add(stage_,std::chrono::duration<double,std::milli>(Clock::now()-start_).count());}
    EntryTimer(const EntryTimer&)=delete;
    EntryTimer& operator=(const EntryTimer&)=delete;
private:
    using Clock=std::chrono::steady_clock;
    SceneEntryTimings* timings_;EntryStage stage_;Clock::time_point start_;
};
inline void WriteSceneEntryTimings(std::ostream& out,const SceneEntryTimings& timings) {
    constexpr std::array names={"plan","map_finalize","character_clips","catalog","remote_world"};
    static_assert(names.size()==size_t(EntryStage::Count));
    for(size_t i=0;i<names.size();++i) {
        const auto& s=timings.stages[i];
        out<<",\"entry_"<<names[i]<<"_calls\":"<<s.calls
           <<",\"entry_"<<names[i]<<"_total_ms\":"<<s.totalMs
           <<",\"entry_"<<names[i]<<"_max_ms\":"<<s.maxMs;
    }
}
}
