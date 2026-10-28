#include "assets/animation_builder.h"
#include "assets/assets_components.h"
namespace cgl {
namespace {
AnimationReaders selectReaders(const AnimationRequest& request,component::AssetsReaderState& readers) {
    if(request.version>=CrossGateVersion::COUNT) throw std::runtime_error("Invalid animation resource version");
    const auto v=request.version;
    return {readers.pAnimeInfoReader[v].get(),readers.pAnimeDataReader[v].get(),
            readers.pGfxInfoReader[v].get(),readers.pGfxDataReader[v].get(),readers.pPaletteReader.get()};
}
}
uint64_t AnimationBuilder::requiredBytes(const AnimationRequest& request,component::AssetsReaderState& readers,const std::atomic_bool* cancelled) {
    std::lock_guard lock(*readers.queryMutex);
    return AnimationDecoder::requiredBytes(request,selectReaders(request,readers),cancelled);
}
AnimationPixels AnimationBuilder::build(const AnimationRequest& request,component::AssetsReaderState& readers,uint64_t bytes,const std::atomic_bool* cancelled) {
    std::lock_guard lock(*readers.queryMutex);
    return AnimationDecoder::build(request,selectReaders(request,readers),bytes,cancelled);
}
}
