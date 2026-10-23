#include "assets/animation_library.h"
#include "assets/assets_components.h"
#include "cgl/graphics/device.h"
#include "cgl/trace/logger.h"
#include <stdexcept>
#include <algorithm>

namespace cgl {
std::shared_ptr<const AnimationClip> AnimationLibrary::acquire(
    const AnimationRequest& request, component::AssetsReaderState& readers,
    graphics::IDevice* device, graphics::IRenderPass* pass) {
    if (!device || !pass || (device_ && device_ != device) || (pass_ && pass_ != pass))
        throw std::invalid_argument("Animation library requires one live device/render pass");
    if (auto found=findOrRestore(request)) return found;
    if(pending_ || decodeRequest_) throw std::runtime_error("Synchronous acquire cannot create another clip while an upload is pending");
    auto clip=buildClip(request,readers,device,pass,false);
    clips_.insert(request,clip,uint64_t(clip->width)*clip->height*clip->frameCount*4);
    return clip;
}
std::shared_ptr<const AnimationClip> AnimationLibrary::acquireForScene(
    const AnimationRequest& request,component::AssetsReaderState& readers,
    graphics::IDevice* device,graphics::IRenderPass* pass) {
    if(!device || !pass || (device_ && device_!=device) || (pass_ && pass_!=pass))
        throw std::invalid_argument("Animation library requires one live device/render pass");
    if(auto found=findOrRestore(request)) return found;
    if(pending_ || decodeRequest_) throw std::logic_error("Scene admission requires pending streaming work to be cancelled");
    if(budgetWait_ && budgetWait_->request==request && remainingBytes()<budgetWait_->requiredBytes) {
        ++budgetDeferrals_;return nullptr;
    }
    budgetWait_.reset();
    try {return acquire(request,readers,device,pass);}
    catch(const AnimationBudgetExceeded& error) {deferBudget(request,error.requiredBytes,AnimationBuilder::MaxBytes);return nullptr;}
}
std::shared_ptr<const AnimationClip> AnimationLibrary::acquireProgressive(
    const AnimationRequest& request,component::AssetsReaderState& readers,
    graphics::IDevice* device,graphics::IRenderPass* pass,bool backgroundDecode) {
    if(!device || !pass || (device_ && device_!=device) || (pass_ && pass_!=pass))
        throw std::invalid_argument("Animation library requires one live device/render pass");
    if(auto found=findOrRestore(request)) return found;
    if(decodeRequest_) {
        if(decodeReaders_!=&readers) throw std::invalid_argument("Cannot change readers during background decode");
        auto result=decoder_.tryTake();
        if(!result) return nullptr;
        const auto completed=*decodeRequest_;decodeRequest_.reset();decodeReaders_=nullptr;
        if(result->error) {
            try {std::rethrow_exception(result->error);}
            catch(const AnimationBudgetExceeded& error) {deferBudget(completed,error.requiredBytes);return nullptr;}
        }
        pending_=std::pair{completed,uploadClip(completed,std::move(result->pixels),device,pass,true)};
        return nullptr; // First upload chunk was just submitted. Never poll twice.
    }
    if(pending_) {
        const auto status=pending_->second->renderer->pollUpload();
        if(status==graphics::SpriteUploadState::Failed) {
            pending_.reset();throw std::runtime_error("Progressive animation upload failed");
        }
        if(status==graphics::SpriteUploadState::Ready) {
            const auto& c=pending_->second;
            clips_.insert(pending_->first,c,uint64_t(c->width)*c->height*c->frameCount*4);pending_.reset();
            if(auto found=findOrRestore(request)) return found;
        }
        return nullptr; // Do not start another submission in this same call.
    }
    if(budgetWait_) {
        if(budgetWait_->request!=request) budgetWait_.reset();
        else if(remainingBytes()<budgetWait_->requiredBytes) {++budgetDeferrals_;return nullptr;}
        else budgetWait_.reset();
    }
    if(backgroundDecode) {
        const auto limit=std::min<uint64_t>(remainingBytes(),AnimationDecodeWorker::MaxBytes);
        if(!limit) {deferBudget(request,1);return nullptr;}
        if(!decoder_.submit([this,&readers,request,limit](const std::atomic_bool& cancelled) {
            return builder_.build(request,readers,limit,&cancelled);
        },limit)) throw std::logic_error("Decoder slot unexpectedly occupied");
        decodeRequest_=request;decodeReaders_=&readers;device_=device;pass_=pass;
        return nullptr;
    }
    try {
        auto clip=buildClip(request,readers,device,pass,true);
        pending_=std::pair{request,std::move(clip)};
    } catch(const AnimationBudgetExceeded& error) {deferBudget(request,error.requiredBytes);}
    return nullptr;
}
std::shared_ptr<const AnimationClip> AnimationLibrary::buildClip(
    const AnimationRequest& request,component::AssetsReaderState& readers,
    graphics::IDevice* device,graphics::IRenderPass* pass,bool progressive) {
    const auto available=remainingBytes();
    auto pixels=builder_.build(request,readers,progressive ? std::min<uint64_t>(available,AnimationDecodeWorker::MaxBytes) : available);
    return uploadClip(request,std::move(pixels),device,pass,progressive);
}
std::shared_ptr<const AnimationClip> AnimationLibrary::findOrRestore(const AnimationRequest& request) {
    if(auto found=clips_.find(request)) return found;
    std::shared_ptr<const AnimationClip> restored;
    retired_.restore(request,[&](const auto& clip,uint64_t bytes) {
        clips_.insert(request,clip,bytes);restored=clip;
    });
    if(restored && budgetWait_ && budgetWait_->request==request) budgetWait_.reset();
    return restored;
}
void AnimationLibrary::deferBudget(const AnimationRequest& request,uint64_t requiredBytes,uint64_t clipLimit) {
    if(requiredBytes>std::min(textureBudget_,clipLimit))
        throw std::runtime_error("Animation exceeds clip/total texture budget");
    budgetWait_=BudgetWait{request,requiredBytes};++budgetDeferrals_;
}
void AnimationLibrary::maintainStreaming(uint64_t submitted,uint64_t completed,bool allowEviction) {
    retired_.collect(completed,8);
    if(pressureCooldown_) {--pressureCooldown_;return;}
    if(!allowEviction || !budgetWait_) return;
    const auto available=remainingBytes();
    if(available>=budgetWait_->requiredBytes) return;
    const auto needed=budgetWait_->requiredBytes-available;
    // Already-retired payload will become available; do not evict more for it.
    if(retired_.bytes()>=needed) return;
    const auto fromReady=needed-retired_.bytes();
    const auto target=clips_.bytes()-std::min(fromReady,clips_.bytes());
    clips_.retireKeyed(target,[]{},[&](const auto& key,const auto& clip,uint64_t bytes) {
        retired_.retain(key,submitted,clip,bytes);
    },std::numeric_limits<size_t>::max(),8);
    pressureCooldown_=15;
}
uint64_t AnimationLibrary::remainingBytes() const {
    const auto cachedBytes=clips_.bytes()+retired_.bytes();
    if(cachedBytes>textureBudget_) throw std::runtime_error("Animation cache exceeds texture budget");
    return textureBudget_-cachedBytes;
}
uint64_t AnimationLibrary::retireUnused(uint64_t targetBytes,uint64_t lastSubmitted) {
    return clips_.retireKeyed(targetBytes,[]{},[&](const auto& key,const auto& clip,uint64_t bytes) {
        retired_.retain(key,lastSubmitted,clip,bytes);
    });
}
uint64_t AnimationLibrary::trimUnused(uint64_t targetBytes) {
    return clips_.trim(targetBytes,[this] {
        if(!device_ || device_->waitIdle()!=Results::Success)
            throw std::runtime_error("Cannot synchronize animation cache eviction");
    });
}
std::shared_ptr<const AnimationClip> AnimationLibrary::uploadClip(const AnimationRequest& request,AnimationPixels pixels,
    graphics::IDevice* device,graphics::IRenderPass* pass,bool progressive) {
    auto renderer = progressive ? graphics::ISpriteRenderer::createPending(device,pass,
        pixels.width,pixels.height,pixels.frameCount,std::move(pixels.rgba)) :
        graphics::ISpriteRenderer::create(device,pass,pixels.width,pixels.height,pixels.frameCount,pixels.rgba);
    if (!renderer) throw std::runtime_error("Could not upload animation or create sprite pipeline");
    auto result = std::make_shared<AnimationClip>();
    result->renderer = std::move(renderer);
    result->offsetX = pixels.offsetX;
    result->offsetY = pixels.offsetY;
    result->width = pixels.width;
    result->height = pixels.height;
    result->frameCount = pixels.frameCount;
    result->durationMs = pixels.durationMs;
    device_ = device;
    pass_ = pass;
    LOGI((progressive ? "Character animation pending: serial=" : "Character animation ready: serial=") << request.serial << ", frames=" << pixels.frameCount
         << ", duration=" << pixels.durationMs << "ms, canvas=" << pixels.width << "x" << pixels.height);
    return result;
}
}
