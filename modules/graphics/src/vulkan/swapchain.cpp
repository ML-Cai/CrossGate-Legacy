// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "cgl/trace/logger.h"
#include "vulkan/device.h"
#include "vulkan/swapchain.h"
#include "vulkan/semaphore.h"
#include "vulkan/common.h"


using cgl::graphics::vulkan::Swapchain;

// -----------------------------------------------------------------------------
namespace {

template<typename T>
inline uint32_t clamp(T v, T vmin, T vmax) {
    return (v < vmin) ? vmin : (v > vmax) ? vmax : v;
}

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
struct SwapChainSupportInfo {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

bool QuerySwapChainSupportInfo(
    VkPhysicalDevice      device,
    VkSurfaceKHR          surface,
    SwapChainSupportInfo* pInfo
) {
    VkResult result = VkResult::VK_SUCCESS;
    if (pInfo == nullptr) {
        return false;
    }

    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                                       &pInfo->capabilities);
    if (result != VK_SUCCESS) return false;


    // query surface format
    uint32_t count = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface,
                                                  &count, nullptr);
    if (result != VK_SUCCESS || count == 0) {
        return false;
    }
    pInfo->formats.resize(count);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(
                device, surface, &count, pInfo->formats.data());
    if (result != VK_SUCCESS) {
        return false;
    }

    // query surface present mode
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
                                                       &count, nullptr);
    if (result != VK_SUCCESS || count == 0) {
        return false;
    }
    pInfo->presentModes.resize(count);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(
                device, surface, &count, pInfo->presentModes.data());

    if (result != VK_SUCCESS) {
        return false;
    }

    return true;
}

bool QuerySupportedSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats,
    VkSurfaceFormatKHR *                   pSurfaceFmt
) {
    for (const auto& availableFormat : availableFormats) {
        if ((availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM)) {
            *pSurfaceFmt = availableFormat;
            return true;
        }
        // if ((availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB) &&
        //     (availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)) {
        //     *pSurfaceFmt = availableFormat;
        //     return true;
        // }
    }
    return false;
}

bool QuerySupportedSwapchainExtent(
    const VkSurfaceCapabilitiesKHR& cap,
    GLFWwindow*                     pWindow,
    VkExtent2D*                     pExtent
) {
    if (cap.currentExtent.width != UINT32_MAX) {
        *pExtent = cap.currentExtent;
    } else if (pWindow != nullptr) {
        int width, height;
        glfwGetFramebufferSize(pWindow, &width, &height);
        pExtent->width = width;
        pExtent->height = height;
        pExtent->width = clamp(pExtent->width,
                               cap.minImageExtent.width,
                               cap.maxImageExtent.width);
        pExtent->height = clamp(pExtent->height,
                                cap.minImageExtent.height,
                                cap.maxImageExtent.height);
    } else {
        return false;
    }
    return true;
}

bool VerifySwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats,
    const VkFormat                         targetFmt,
    const VkColorSpaceKHR                  targetColorSpace
) {
    for (const auto& format : formats) {
        if (format.format == targetFmt &&
            format.colorSpace == targetColorSpace) {
            return true;
        }
    }
    return false;
}

bool VerifySwapPresentMode(
    const std::vector<VkPresentModeKHR>& modes,
    const VkPresentModeKHR               targetMode
) {
    for (const auto& mode : modes) {
        if (mode == targetMode) {
            return true;
        }
    }
    return false;
}

}   // namespace

// -----------------------------------------------------------------------------
// cgl::graphics::vulkan::Swapchain
// -----------------------------------------------------------------------------
Swapchain::Swapchain(cgl::graphics::IDevice* pDevice,
                     void* pWindowNativeHandle)
    : swapChain_(VK_NULL_HANDLE),
      curImageIndex_(0) {
    pNativeDevice_ = reinterpret_cast<cgl::graphics::vulkan::Device *>(pDevice);
}

Swapchain::~Swapchain() {
    destroy();
}

void Swapchain::destroy() {
    for (auto imageView : swapChainImageViews_) {
        vkDestroyImageView(pNativeDevice_->device(), imageView, nullptr);
    }

    if (swapChain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(pNativeDevice_->device(), swapChain_, nullptr);
        swapChain_ = nullptr;
    }
}

bool Swapchain::prepare(void* pWindowNativeHandle) {
    LOGI("Prepare Swapchain(Vulkan) ...");

    if (!createSwapChain(pWindowNativeHandle)) return false;
    if (!createSwapChainImageViews()) return false;

    return true;
}

bool Swapchain::createSwapChain(void* pWindowNativeHandle) {
    LOGD("Create swapchain ...");

    SwapChainSupportInfo supportInfo;
    if (!QuerySwapChainSupportInfo(pNativeDevice_->physicalDevice(),
                                   pNativeDevice_->surface(),
                                   &supportInfo)) {
        LOGE("Fail to query swapchain support info");
        return false;
    }

    uint32_t imageCount = supportInfo.capabilities.minImageCount + 1;
    LOGD("Swapchain image count: " << imageCount);
    if (supportInfo.capabilities.maxImageCount > 0 &&
        imageCount > supportInfo.capabilities.maxImageCount) {
        imageCount = supportInfo.capabilities.maxImageCount;
    }

    auto pWinHandle = reinterpret_cast<GLFWwindow*>(pWindowNativeHandle);
    if (!QuerySupportedSwapchainExtent(supportInfo.capabilities,
                                       pWinHandle,
                                       &extent_)) {
        LOGE("Fail to query swap chain extent");
        return false;
    }
    LOGD("Framebuffer extent: " << extent_.width << "x" << extent_.height);

    if (!QuerySupportedSwapSurfaceFormat(supportInfo.formats, &surfaceFmt_)) {
        LOGE("Fail to query surface format");
        return false;
    }
    LOGD("Surface format: " << string_VkFormat(surfaceFmt_.format)
         << "/" << string_VkColorSpaceKHR(surfaceFmt_.colorSpace));

    uint32_t queueFamilyIndices[] = {pNativeDevice_->graphicsQueueFamily(),
                                     pNativeDevice_->presentQueueFamily()};

    VkSwapchainCreateInfoKHR createInfo {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = pNativeDevice_->surface(),
        .minImageCount    = imageCount,
        .imageFormat      = surfaceFmt_.format,
        .imageColorSpace  = surfaceFmt_.colorSpace,
        .imageExtent      = extent_,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform     = supportInfo.capabilities.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = VK_PRESENT_MODE_FIFO_KHR,
        .clipped          = VK_TRUE,
        .oldSwapchain     = VK_NULL_HANDLE,
    };

    if (pNativeDevice_->graphicsQueueFamily() != pNativeDevice_->presentQueueFamily()) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    if (!VerifySwapPresentMode(supportInfo.presentModes,
                               createInfo.presentMode)) {
        LOGE("Failed to choose the unsupported swap present mode:"
             << string_VkPresentModeKHR(createInfo.presentMode));
    }

    if (!VerifySwapSurfaceFormat(supportInfo.formats,
                                 createInfo.imageFormat,
                                 createInfo.imageColorSpace)) {
        LOGE("Failed to choose the unsupported swap surface format: "
             << string_VkFormat(createInfo.imageFormat) << " / "
             << string_VkColorSpaceKHR(createInfo.imageColorSpace));
    }

    VkResult result = vkCreateSwapchainKHR(pNativeDevice_->device(),
                                           &createInfo,
                                           nullptr,
                                           &swapChain_);
    if (result != VK_SUCCESS) {
        LOGE("Failed to create swap chain, error code:" << string_VkResult(result));
        return false;
    }

    return true;
}

bool Swapchain::createSwapChainImageViews() {
    LOGD("Create swapchain image views ...");

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(pNativeDevice_->device(),
                            swapChain_,
                            &imageCount,
                            nullptr);
    swapChainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(pNativeDevice_->device(),
                            swapChain_,
                            &imageCount,
                            swapChainImages_.data());

    swapChainImageViews_.resize(swapChainImages_.size());

    for (size_t i = 0; i < swapChainImages_.size(); i++) {
        VkImageViewCreateInfo createInfo {
            .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image      = swapChainImages_[i],
            .viewType   = VK_IMAGE_VIEW_TYPE_2D,
            // .format     = VK_FORMAT_B8G8R8A8_SRGB,
            .format     = VK_FORMAT_B8G8R8A8_UNORM,
            .components = VkComponentMapping {
                            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .a = VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = VkImageSubresourceRange {
                                 .aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel    = 0,
                                 .levelCount      = 1,
                                 .baseArrayLayer  = 0,
                                 .layerCount      = 1}
        };

        if (vkCreateImageView(pNativeDevice_->device(),
                              &createInfo,
                              nullptr,
                              &swapChainImageViews_[i]) != VK_SUCCESS) {
            LOGE("failed to create image views!");
            return false;
        }
    }

    return true;
}

bool Swapchain::acquireNextImageIndex(
    cgl::graphics::ISemaphore* pSemaphore,
    uint32_t*                  pImageIndex
) {
    // check input
    if (pSemaphore == nullptr) {
        LOGE("Invalid inputs");
        return false;
    }

    auto pVkSemaphore = static_cast<cgl::graphics::vulkan::Semaphore *>(pSemaphore);
    VkResult result = vkAcquireNextImageKHR(
                        pNativeDevice_->device(),
                        swapChain_,
                        UINT64_MAX,
                        pVkSemaphore->semaphore(),
                        VK_NULL_HANDLE,
                        &curImageIndex_);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        LOGE("Handle windows resize logic here");
        exit(1);
    }

    if (pImageIndex != nullptr) *pImageIndex = curImageIndex_;

    return result == VK_SUCCESS;
}

uint32_t Swapchain::acquireCurrentImageIndex() const {
    return curImageIndex_;
}
