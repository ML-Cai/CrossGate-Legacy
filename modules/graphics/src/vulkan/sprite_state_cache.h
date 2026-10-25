#pragma once
#include <vulkan/vulkan_core.h>
#include "cgl/common/viewport.h"

namespace cgl::graphics::vulkan {
// Recording-local scalar keys, not resource owners. Native commands outside the
// tracked path must invalidate these keys before another tracked sprite draw.
class SpriteStateCache final {
public:
    struct Changes {bool pipeline,descriptor,viewport;};
    Changes update(VkPipeline pipeline,VkPipelineLayout layout,VkDescriptorSet descriptor,const cgl::Viewport& viewport) noexcept {
        const Changes changes{pipeline_!=pipeline || !valid_,
            !valid_ || pipeline_!=pipeline || layout_!=layout || descriptor_!=descriptor,
            !valid_ || viewport_!=viewport};
        pipeline_=pipeline;layout_=layout;descriptor_=descriptor;viewport_=viewport;valid_=true;
        return changes;
    }
    void invalidate() noexcept { *this=SpriteStateCache{}; }
private:
    VkPipeline pipeline_=VK_NULL_HANDLE;
    VkPipelineLayout layout_=VK_NULL_HANDLE;
    VkDescriptorSet descriptor_=VK_NULL_HANDLE;
    cgl::Viewport viewport_{};
    bool valid_=false;
};
}
