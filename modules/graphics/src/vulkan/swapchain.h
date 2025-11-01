// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <vulkan/vulkan_core.h>
#include <unordered_map>
#include <memory>
#include <vector>
#include "cgl/graphics/swapchain.h"

namespace cgl {
namespace vk {

class Device;

class Swapchain : public cgl::ISwapchain {
 public:
    explicit Swapchain(cgl::IDevice* pDevice,
                       void* pWindowNativeHandle);

    virtual ~Swapchain();

    void destroy();

    bool prepare(void* pWindowNativeHandle);

    uint32_t imageCount() const override {
        return static_cast<uint32_t>(swapChainImages_.size());
    }

    cgl::Size2u extent() const override {
        return cgl::Size2u(extent_.width, extent_.height);
    }

    bool acquireNextImageIndex(
        cgl::ISemaphore* pSemaphore,
        uint32_t*        pImageIndex = nullptr) override;

    uint32_t acquireCurrentImageIndex() const override;

    VkFormat imageFormat() const {
        return surfaceFmt_.format;
    }

    VkImage swapChainImage(uint32_t idx) const {
        return swapChainImages_[idx];
    }

    VkImageView swapChainImageView(uint32_t idx) const {
        return swapChainImageViews_[idx];
    }

    VkSwapchainKHR swapChain() const { return swapChain_; }

 private:
    cgl::vk::Device* pNativeDevice_;
    uint32_t curImageIndex_;
    VkSwapchainKHR swapChain_;
    VkExtent2D extent_;
    VkSurfaceFormatKHR surfaceFmt_;
    std::vector<VkImage> swapChainImages_;
    std::vector<VkImageView> swapChainImageViews_;

    bool createSwapChain(void* pWindowNativeHandle);
    bool createSwapChainImageViews();
};

}   // namespace vk
}   // namespace cgl
