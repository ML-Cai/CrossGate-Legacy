#pragma once

#include "assets/animation_builder.h"
#include "assets/animation_decode_worker.h"
#include "assets/idle_resource_cache.h"
#include "assets/keyed_retirement_cache.h"
#include <memory>
#include <chrono>
#include "telemetry/load_telemetry.h"
#include <stdexcept>
#include "cgl/graphics/sprite_renderer.h"

namespace cgl {
namespace component { struct AssetsReaderState; }
// Immutable shared clip. Playback state belongs to individual entities.
struct AnimationClip {
    graphics::ISpriteRenderer::Ptr renderer;
    uint32_t width = 0, height = 0, frameCount = 0, durationMs = 0;
    int32_t offsetX = 0, offsetY = 0;
};
// Owned by ClientEngine. Current cache uses version/serial/direction/motion, daytime resources and one
// device/render pass. Clear after GPU completion, before destroying the device.
class AnimationLibrary {
public:
    ~AnimationLibrary() { clear(); }
    std::shared_ptr<const AnimationClip> acquire(const AnimationRequest& request,
        component::AssetsReaderState& readers, graphics::IDevice* device, graphics::IRenderPass* pass);
    // Serialized render-thread path: returns nullptr until upload completion.
    // At most one pending clip (<=32 MiB decoded RGBA, <=4 MiB staging).
    // Different requests return nullptr while that slot is occupied; retry later.
    // Background metadata/decode is enabled by default; poll once per frame.
    // Metadata occupies the slot with zero pixel bytes; cpuPending is authoritative.
    // Reader storage must outlive clear()/destruction. All GPU work stays here.
    std::shared_ptr<const AnimationClip> acquireProgressive(const AnimationRequest& request,
        component::AssetsReaderState& readers, graphics::IDevice* device, graphics::IRenderPass* pass, bool backgroundDecode=true);
    // Scene entry may defer synchronous acquisition while older ownership occupies
    // capacity. It supports the normal 256 MiB clip limit, without a worker job.
    std::shared_ptr<const AnimationClip> acquireForScene(const AnimationRequest& request,
        component::AssetsReaderState& readers,graphics::IDevice* device,graphics::IRenderPass* pass);
    std::shared_ptr<const AnimationClip> findReady(const AnimationRequest& request) const {
        return clips_.find(request);
    }
    uint64_t pendingBytes() const noexcept {
        if(!pending_) return decoder_.reservedBytes();
        const auto& c=pending_->second;return uint64_t(c->width)*c->height*c->frameCount*4;
    }
    struct Usage {size_t clips=0;uint64_t rgbaBytes=0,pendingBytes=0,budgetBytes=0,evictions=0,evictedBytes=0,retiredBytes=0;size_t retiredClips=0;uint64_t budgetDeferrals=0;bool waitingForBudget=false;uint64_t reusedClips=0;};
    Usage usage() const noexcept {
        return {clips_.size(),clips_.bytes(),pendingBytes(),textureBudget_,clips_.evictions(),clips_.evictedBytes(),retired_.bytes(),retired_.size(),budgetDeferrals_,bool(budgetWait_),retired_.restores()};
    }
    bool cpuPending() const noexcept {return bool(decodeRequest_);}
    bool loading(const AnimationRequest& request) const noexcept {
        return (!decodeDiscarding_ && decodeRequest_ && *decodeRequest_==request) ||
            (pending_ && pending_->first==request);
    }
    bool streamingPending() const noexcept {return bool(decodeRequest_) || bool(pending_);}
    LoadSummary loadTimings() const {return loadTimings_.summary();}
    uint64_t cancelledLoads() const noexcept {return cancelledLoads_;}
    uint64_t failedLoads() const noexcept {return failedLoads_;}
    // Cancellation/scene exit can wait for the one already-submitted chunk.
    void cancelPending() noexcept { cancelLoadTiming();budgetWait_.reset(); decoder_.cancel(); decodeDiscarding_=false; decodeRequest_.reset(); decodeReaders_=nullptr; pending_.reset(); builder_.clear(); }
    // A running metadata/decode job uses an admission snapshot: shrinking waits
    // until the CPU slot is collected/cancelled. Increasing is safe (retry if needed).
    void setTextureBudget(uint64_t bytes) {
        const uint64_t used=clips_.bytes()+retired_.bytes()+pendingBytes();
        if ((decodeRequest_ && bytes<textureBudget_) || !bytes || used > bytes || (budgetWait_ && budgetWait_->requiredBytes>bytes)) throw std::invalid_argument("Texture budget below live payload/reservation or waiting clip minimum");
        textureBudget_ = bytes;
    }
    // Blocking maintenance only: waits for this device if idle victims exist.
    // Held clips and in-flight decode/upload slots are never eviction candidates.
    uint64_t trimUnused(uint64_t targetBytes);
    // Caller provides this renderer's last successful graphics submission.
    // Only ready/cache-only entries move; GPU lifetime continues in retired_.
    uint64_t retireUnused(uint64_t targetBytes,uint64_t lastSubmitted);
    uint64_t collectRetired(uint64_t completed,size_t maxEntries=std::numeric_limits<size_t>::max()) noexcept {
        return retired_.collect(completed,maxEntries);
    }
    // Cancel only CPU/budget work without waiting. Submitted GPU chunks are never
    // cancelled by this path. Reservation stays charged until worker completion.
    bool discardUnsubmitted() noexcept;
    bool uploadPending() const noexcept {return bool(pending_);}
    uint64_t discardedDecodes() const noexcept {return discardedDecodes_;}
    bool waitingForBudget() const noexcept {return bool(budgetWait_);}
    std::optional<uint64_t> waitingBytes(const AnimationRequest& request) const noexcept {
        return budgetWait_ && budgetWait_->request==request ?
            std::optional<uint64_t>(budgetWait_->requiredBytes) : std::nullopt;
    }
    // Owner-thread O(cache size) planning hint. Includes idle/retired payload that
    // may eventually retire, never credits it to actual admission before fences.
    // Call at a throttled policy boundary, not for every actor or every frame.
    uint64_t eventualAdmissionBytes() const noexcept {
        const auto held=clips_.bytes()-clips_.idleBytes()+pendingBytes();
        return held<textureBudget_ ? textureBudget_-held : 0;
    }
    // Render-thread maintenance: <=8 completed clip destructions, <=8 retirements
    // per scan. Full LRU scans are throttled to once every 16 update calls.
    void maintainStreaming(uint64_t submitted,uint64_t completed,bool allowEviction=true);
    // Release scene/entity references and wait for GPU work before clearing.
    void clear() noexcept {
        loadStarted_.reset();loadTimings_.clear();cancelledLoads_=failedLoads_=0;
        budgetWait_.reset();budgetDeferrals_=0;pressureCooldown_=0;
        decoder_.stop(); decodeDiscarding_=false; discardedDecodes_=0; decodeRequest_.reset(); decodeReaders_=nullptr;
        pending_.reset(); // Waits for only its own submitted chunk, if any.
        clips_.clear();
        retired_.clear();
        builder_.clear();
        device_ = nullptr;
        pass_ = nullptr;
    }
private:
    using Clock=std::chrono::steady_clock;
    struct StartedLoad {AnimationRequest request;Clock::time_point start,uploadStart{};double cpuMs=0;};
    std::optional<StartedLoad> loadStarted_;
    LoadTelemetry loadTimings_;
    uint64_t cancelledLoads_=0,failedLoads_=0;
    void cancelLoadTiming() noexcept {if(loadStarted_) {++cancelledLoads_;loadStarted_.reset();}}
    void finishLoadTiming();
    struct BudgetWait {AnimationRequest request;uint64_t requiredBytes;};
    std::optional<BudgetWait> budgetWait_;
    uint64_t budgetDeferrals_=0;
    uint32_t pressureCooldown_=0;
    std::shared_ptr<const AnimationClip> findOrRestore(const AnimationRequest& request);
    void deferBudget(const AnimationRequest& request,uint64_t requiredBytes,uint64_t clipLimit=AnimationDecodeWorker::MaxBytes);
    uint64_t remainingBytes() const;
    std::shared_ptr<const AnimationClip> uploadClip(const AnimationRequest& request,AnimationPixels pixels,
        graphics::IDevice* device,graphics::IRenderPass* pass,bool progressive);
    bool collectDiscardedDecode() noexcept;
    bool decodeDiscarding_=false;
    uint64_t discardedDecodes_=0;
    AnimationDecodeWorker decoder_;
    std::optional<AnimationRequest> decodeRequest_;
    component::AssetsReaderState* decodeReaders_=nullptr;
    std::shared_ptr<const AnimationClip> buildClip(const AnimationRequest& request,
        component::AssetsReaderState& readers, graphics::IDevice* device, graphics::IRenderPass* pass, bool progressive);
    std::optional<std::pair<AnimationRequest,std::shared_ptr<const AnimationClip>>> pending_;
    uint64_t textureBudget_ = 256ull*1024*1024;
    AnimationBuilder builder_;
    IdleResourceCache<AnimationRequest,AnimationClip> clips_;
    KeyedRetirementCache<AnimationRequest,AnimationClip> retired_;
    graphics::IDevice* device_ = nullptr;
    graphics::IRenderPass* pass_ = nullptr;
};
}
