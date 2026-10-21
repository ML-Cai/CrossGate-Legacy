#pragma once
#include <vulkan/vulkan_core.h>

namespace cgl::graphics::vulkan {
// One immutable bundle per render-pass object. Pass and renderers share ownership;
// all owners must be released after GPU completion and before device destruction.
struct SpritePipeline final {
    explicit SpritePipeline(VkDevice device) : device_(device) {}
    ~SpritePipeline();
    SpritePipeline(const SpritePipeline&) = delete;
    SpritePipeline& operator=(const SpritePipeline&) = delete;
    bool prepare(VkRenderPass pass);
    VkDevice device_;
    VkSampler sampler_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
};
}
