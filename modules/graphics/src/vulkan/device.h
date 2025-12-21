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
#include <memory>
#include <vector>
#include <filesystem>
#include "cgl/graphics/device.h"

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
