// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <vulkan/vulkan_core.h>
#include <string>
#include <atomic>
#include <memory>
#include <vector>
#include <filesystem>
#include "cgl/graphics/device.h"
#include "sprite_memory_ledger.h"

namespace cgl {
namespace graphics {
namespace vulkan {

struct QueueFamilyIndices {
    uint32_t graphicsFamily;
    uint32_t presentFamily;

    QueueFamilyIndices()
        : graphicsFamily(UINT32_MAX), presentFamily(UINT32_MAX) {}

    bool isComplete() {
        return (graphicsFamily != UINT32_MAX) &&
               (presentFamily != UINT32_MAX);
    }
};


class Device : public cgl::graphics::IDevice  {
 public:
    using Ptr = std::unique_ptr<cgl::graphics::IDevice>;

    explicit Device(bool enableDebug);

    virtual ~Device();

    void destroy();

    bool prepare(void* windowNativeHandle);

    bool getLastError(std::string* pError = nullptr) override;

    void onValidationLayerCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severityFlag,
        VkDebugUtilsMessageTypeFlagsEXT typeFlag,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData);

    // Counts validation errors, including repeated identical messages. General
    // loader diagnostics (e.g. stale layer manifests) are tracked by the logger.
    uint64_t validationErrorCount() const noexcept {
        return validationErrors_.load(std::memory_order_relaxed);
    }

    std::optional<SpriteAllocationStatistics> spriteAllocationStatistics() const noexcept override {return spriteMemory_.snapshot();}
    bool recordSpriteAllocation(SpriteAllocationKind kind,uint64_t bytes) noexcept {return spriteMemory_.allocate(kind,bytes);}
    bool releaseSpriteAllocation(SpriteAllocationKind kind,uint64_t bytes) noexcept {return spriteMemory_.release(kind,bytes);}

    VkSurfaceKHR surface() const { return surface_; }

    VkPhysicalDevice physicalDevice() const { return physicalDevice_; }

    VkPhysicalDeviceProperties physicalDeviceProperties() const {
        return physicalDeviceProperties_;
    }

    VkDevice device() const { return device_; }

    VkDevice handle() const { return device_; }

    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties() const {
        return physicalDeviceMemProperties_;
    }

    uint32_t graphicsQueueFamily() const {
        return queueFamilyIndices_.graphicsFamily;
    }

    uint32_t presentQueueFamily() const {
        return queueFamilyIndices_.presentFamily;
    }

    cgl::graphics::IQueue* presentQueue() override {
        return presentQueue_.get();
    }

    cgl::graphics::IQueue* graphicsQueue() override {
        return graphicsQueue_.get();
    }

    cgl::Results waitIdle() override;

    bool create2DImage(const uint32_t              width,
                       const uint32_t              height,
                       const VkFormat              format,
                       const uint32_t              mipLevels,
                       const VkImageTiling         tiling,
                       const VkImageUsageFlags     usage,
                       const VkMemoryPropertyFlags properties,
                       VkImage*                    pImage,
                       VkDeviceMemory*             pImageMemory);

    bool create2DImageView(const VkImage            image,
                           const VkFormat           format,
                           const uint32_t           mipLevels,
                           const VkImageAspectFlags aspectFlags,
                           VkImageView*             pImageView);

    bool createShaderModule(const std::filesystem::path& path,
                            VkShaderModule*              pModule);

 private:
    SpriteMemoryLedger spriteMemory_;
    std::atomic<uint64_t> validationErrors_{0};
    bool enableDebug_;
    std::string last_error_;
    VkInstance instance_;
    VkDebugUtilsMessengerEXT debugMessenger_;
    VkSurfaceKHR surface_;
    VkPhysicalDevice physicalDevice_;
    VkPhysicalDeviceProperties physicalDeviceProperties_;
    VkPhysicalDeviceMemoryProperties physicalDeviceMemProperties_;
    VkDevice device_;

    std::unique_ptr<cgl::graphics::IQueue> graphicsQueue_;
    std::unique_ptr<cgl::graphics::IQueue> presentQueue_;

    cgl::graphics::vulkan::QueueFamilyIndices queueFamilyIndices_;

    bool createInstance();
    bool setupValidationLayer();
    bool createSurface(void* windowNativeHandle);
    bool createPhysicalDevice();
    bool createLogicalDevice();

    VkResult findMemoryTypeIndex(
        uint32_t              typeFilter,
        VkMemoryPropertyFlags properties,
        uint32_t*             pIndex) const;
};


}   // namespace vulkan
}   // namespace graphics
}   // namespace cgl
