#pragma once
#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <stdexcept>
#include <vector>
#include "cgl/settings/settings.h"
#include "cgl/common/assets.h"

namespace cgl { struct AnimationClip; }
namespace cgl::component {
struct CrowdActor { std::shared_ptr<const AnimationClip> idle, walk; size_t resource = 0; };
struct CrowdBenchmark {
    bool ready = false;
    uint32_t actors = 0, unique = 0, visibleCells = 0, drawnActors = 0, drawCalls = 0, filtered = 0;
    uint32_t width=0, height=0, switches=0;
    uint64_t animationBytes = 0;
    double loadMs = 0, sortMs = 0;
    std::vector<double> coldLoadMs, coldReadyMs;
    std::vector<std::pair<uint32_t,uint32_t>> resources;
};
}
namespace cgl {
// Sequential resource coverage with a per-cell rotation after the natural
// modulo period. All requested resources occur before any extra rotation;
// each cell's first min(unique,10) actors remain distinct even when cell/asset
// counts share a divisor.
inline bool IsCrowdCharacterDescription(const AnimeResourceData& data) {
    for (unsigned d=0;d<8;++d) for (unsigned m=0;m<2;++m) {
        auto it=data.motionMap.find({DirectionTypes(d),MotionTypes(m)});
        if(it==data.motionMap.end() || !it->second.duration || it->second.motionGraphicsSerialNums.empty()) return false;
    }
    return true;
}
inline size_t CrowdResourceIndex(size_t actor, size_t cells, size_t unique) {
    if (!cells || !unique) throw std::invalid_argument("Empty crowd resource/cell set");
    const size_t layer=actor/cells,period=unique/std::gcd(cells,unique);
    return (actor+layer/period)%unique;
}
struct CrowdSamples {
    std::vector<double> frame, update, render, sort, coldFrame;
    size_t seen = 0;
    void add(double frameMs, double updateMs, double renderMs, double sortMs, uint32_t warmup, bool coldActive=false) {
        if (seen++ < warmup) return;
        if(coldActive) coldFrame.push_back(frameMs);
        frame.push_back(frameMs); update.push_back(updateMs); render.push_back(renderMs); sort.push_back(sortMs);
    }
    static double mean(const std::vector<double>& v) {
        return v.empty() ? 0 : std::accumulate(v.begin(),v.end(),0.0)/v.size();
    }
    static double percentile(std::vector<double> v, double p) {
        if (v.empty()) return 0;
        std::sort(v.begin(),v.end());
        return v[size_t(std::ceil(p*v.size()))-1];
    }
    void write(const Settings& settings, const component::CrowdBenchmark& state,
               const char* mode, const char* status) const {
        std::ofstream out(settings.benchmark.ReportPath);
        if (!out) throw std::runtime_error("Cannot open benchmark report");
        const double avg = mean(frame);
        out << "{\n\"status\":\"" << status << "\",\n\"present_mode\":\"" << mode
            << "\",\n\"validation\":" << (settings.display.Validation ? "true" : "false")
            << ",\n\"width\":" << state.width << ",\"height\":" << state.height
            << ",\n\"actors\":" << state.actors << ",\"unique_resources\":" << state.unique
            << ",\"filtered_resources\":" << state.filtered
            << ",\"visible_cells\":" << state.visibleCells << ",\"drawn_actors_last_frame\":" << state.drawnActors
            << ",\"draw_calls_last_frame\":" << state.drawCalls
            << ",\"motion_switches\":" << state.switches
            << ",\"switch_motions\":" << (settings.benchmark.SwitchMotions?"true":"false")
            << ",\n\"requested_unique\":" << settings.benchmark.UniqueCharacters
            << ",\"actors_per_cell\":" << settings.benchmark.ActorsPerCell
            << ",\"animation_rgba_payload_bytes\":" << state.animationBytes
            << ",\"animation_budget_mib\":" << settings.benchmark.AnimationBudgetMiB
            << ",\n\"scene_load_ms\":" << state.loadMs << ",\"sample_frames\":" << frame.size()
            << ",\"warmup_frames\":" << settings.benchmark.WarmupFrames
            << ",\n\"fps\":" << (avg>0 ? 1000/avg : 0) << ",\"frame_mean_ms\":" << avg
            << ",\"frame_p50_ms\":" << percentile(frame,.5) << ",\"frame_p95_ms\":" << percentile(frame,.95)
            << ",\"frame_p99_ms\":" << percentile(frame,.99) << ",\"frame_max_ms\":" << percentile(frame,1)
            << ",\n\"frames_over_33ms\":" << std::count_if(frame.begin(),frame.end(),[](double t){return t>1000.0/30;})
            << ",\"update_mean_ms\":" << mean(update) << ",\"render_loop_mean_ms\":" << mean(render)
            << ",\"sort_mean_ms\":" << mean(sort) << ",\n\"resources\":[";
        for(size_t i=0;i<state.resources.size();++i) {
            if(i) out << ',';
            out << '[' << state.resources[i].first << ',' << state.resources[i].second << ']';
        }
        out << "],\n\"cold_load_count\":" << state.coldLoadMs.size()
            << ",\"cold_load_p99_ms\":" << percentile(state.coldLoadMs,.99)
            << ",\"cold_load_max_ms\":" << percentile(state.coldLoadMs,1)
            << ",\"cold_frame_count\":" << coldFrame.size()
            << ",\"cold_frame_p99_ms\":" << percentile(coldFrame,.99)
            << ",\"cold_frame_max_ms\":" << percentile(coldFrame,1)
            << ",\"cold_ready_p99_ms\":" << percentile(state.coldReadyMs,.99)
            << ",\"cold_ready_max_ms\":" << percentile(state.coldReadyMs,1)
            << ",\"progressive_uploads\":" << (settings.benchmark.ProgressiveUploads?"true":"false")
            << ",\"load_during_playback\":" << (settings.benchmark.LoadDuringPlayback?"true":"false")
            << ",\"frame_times_ms\":[";
        for(size_t i=0;i<frame.size();++i) { if(i) out << ','; out << frame[i]; }
        out << "]\n}\n";
        out.flush();
        if (!out) throw std::runtime_error("Could not write benchmark report");
    }
};
}
