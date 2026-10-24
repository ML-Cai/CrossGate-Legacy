#include <gtest/gtest.h>
#include <type_traits>
#include <cstdint>
#include "vulkan/sprite_state_cache.h"
namespace {
template<class T> T key(uintptr_t n) {
    if constexpr(std::is_pointer_v<T>) return reinterpret_cast<T>(n);
    else return static_cast<T>(n);
}
using cgl::graphics::vulkan::SpriteStateCache;
void changes(SpriteStateCache::Changes c,bool pipeline,bool descriptor,bool viewport) {
    EXPECT_EQ(c.pipeline,pipeline);EXPECT_EQ(c.descriptor,descriptor);EXPECT_EQ(c.viewport,viewport);
}
}
TEST(SpriteStateCache, ConsecutiveSpritesShareOnlyUnchangedBindings) {
    SpriteStateCache state;const cgl::Viewport view{0,0,800,600};
    const auto p=key<VkPipeline>(1),q=key<VkPipeline>(2);
    const auto l=key<VkPipelineLayout>(3),m=key<VkPipelineLayout>(4);
    const auto a=key<VkDescriptorSet>(5),b=key<VkDescriptorSet>(6);
    changes(state.update(p,l,a,view),true,true,true);
    changes(state.update(p,l,a,view),false,false,false);
    changes(state.update(p,l,b,view),false,true,false);
    changes(state.update(p,l,a,view),false,true,false); // A/B/A never shares texture state.
    changes(state.update(q,l,a,view),true,true,false);
    changes(state.update(q,m,a,view),false,true,false); // Conservative layout change.
}
TEST(SpriteStateCache, EveryViewportFieldAndInvalidationRequireFreshState) {
    SpriteStateCache state;const auto p=key<VkPipeline>(1);const auto l=key<VkPipelineLayout>(2);const auto d=key<VkDescriptorSet>(3);
    cgl::Viewport view{0,0,800,600};changes(state.update(p,l,d,view),true,true,true);
    for(const cgl::Viewport next:{cgl::Viewport{1,0,800,600},{1,2,800,600},{1,2,640,600},{1,2,640,480}}) {
        changes(state.update(p,l,d,next),false,false,true);view=next;
    }
    state.invalidate();changes(state.update(p,l,d,view),true,true,true);
    state.invalidate();state.invalidate();changes(state.update(p,l,d,view),true,true,true);
}
TEST(SpriteStateCache, RecordingSlotsDoNotShareKeysOrResourceOwnership) {
    SpriteStateCache a,b;const auto p=key<VkPipeline>(1);const auto l=key<VkPipelineLayout>(2);const auto d=key<VkDescriptorSet>(3);
    const cgl::Viewport view{0,0,32,32};
    changes(a.update(p,l,d,view),true,true,true);changes(b.update(p,l,d,view),true,true,true);
    a.invalidate();changes(b.update(p,l,d,view),false,false,false);
    changes(a.update(p,l,d,view),true,true,true);
}
