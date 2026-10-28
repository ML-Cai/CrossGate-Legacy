#pragma once
#include "cgl/assets/animation_decoder.h"
namespace cgl {
namespace component { struct AssetsReaderState; }
// Engine adapter: retains the shared reader lock while using the CPU decoder.
class AnimationBuilder : public AnimationDecoder {
public:
    uint64_t requiredBytes(const AnimationRequest&,component::AssetsReaderState&,const std::atomic_bool* cancelled=nullptr);
    AnimationPixels build(const AnimationRequest&,component::AssetsReaderState&,uint64_t availableBytes=MaxBytes,const std::atomic_bool* cancelled=nullptr);
};
}
