// -----------------------------------------------------------------------------
//   The MIT License (MIT)
//
//   Copyright (c) 2024 MengLun,Cai
//
//   All rights reserved.
//------------------------------------------------------------------------------

#include <fstream>
#include <set>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "cgl/trace/logger.h"
#include "cgl/graphics/queue.h"
#include "vulkan/common.h"
#include "vulkan/device.h"
#include "vulkan/queue.h"

using cgl::vk::Device;

// -----------------------------------------------------------------------------
namespace {

template<typename T>
inline uint32_t to_uint32(T t) {
    return static_cast<uint32_t>(t);
}

template<typename T>
inline uint32_t clamp(T v, T vmin, T vmax) {
    return (v < vmin) ? vmin : (v > vmax) ? vmax : v;
}

const std::vector<const char*> REQUIRED_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};
#define VK_LAYER_KHRONOS_VALIDATION "VK_LAYER_KHRONOS_validation"

const std::vector<const char*> REQUIRED_DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

std::vector<const char*> GetRequiredExtensions(
    bool enableDebug
) {
    uint32_t count = 0;
    const char** etxs = glfwGetRequiredInstanceExtensions(&count);
    std::vector<const char*> extensions(etxs, etxs + count);

    if (enableDebug) {
        extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severityFlag,
    VkDebugUtilsMessageTypeFlagsEXT             typeFlag,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*                                       pUserDataPtr
) {
    if (pUserDataPtr != nullptr) {
        auto pDev = reinterpret_cast<cgl::vk::Device *>(pUserDataPtr);
        pDev->onValidationLayerCallback(severityFlag, typeFlag, pCallbackData);
    }

    return VK_FALSE;
}

void AppendDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT* dbgInfo,
    void*                               pUserDataPtr
) {
    dbgInfo->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    dbgInfo->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dbgInfo->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dbgInfo->pfnUserCallback = DebugCallback;
    dbgInfo->pUserData = pUserDataPtr;
}

bool QuaryGraphicQueueFamilies(
    VkPhysicalDevice             device,
    VkSurfaceKHR                 surface,
    cgl::vk::QueueFamilyIndices* pIndices
) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);

    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0 ; i < families.size() ; i++) {
        // check presetn support or not
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface,
                                             &presentSupport);
        if (presentSupport) {
            pIndices->presentFamily = i;
        }

        // check ququ type
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            pIndices->graphicsFamily = i;
        }

        // Get first one of compatiable family
        if (pIndices->isComplete()) {
            break;
        }
    }

    return pIndices->isComplete();
}

bool CheckDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);

    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, exts.data());

    std::set<std::string> requiredExtensions(REQUIRED_DEVICE_EXTENSIONS.begin(),
                                             REQUIRED_DEVICE_EXTENSIONS.end());
    for (const auto& ext : exts) {
        requiredExtensions.erase(ext.extensionName);
    }

    return requiredExtensions.empty();
}

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

bool checkDeviceSuitable(
    VkPhysicalDevice phyDevice,
    VkSurfaceKHR     surface,
    std::string*     pErrorMsg
) {
    cgl::vk::QueueFamilyIndices indices;
    if (!QuaryGraphicQueueFamilies(phyDevice, surface, &indices)) {
        *pErrorMsg = "Fail to query graphics queue families";
        return false;
    }

    if (!CheckDeviceExtensionSupport(phyDevice)) {
        *pErrorMsg = "Some of essential device extension no support";
        return false;
    }

    SwapChainSupportInfo info;
    if (!QuerySwapChainSupportInfo(phyDevice, surface, &info)) {
        *pErrorMsg = "Fail to query swapchain support info";
        return false;
    }

    return true;
}

}   // namespace

// -----------------------------------------------------------------------------
// cgl::vk::Device
// -----------------------------------------------------------------------------
Device::Device(bool enableDebug)
    : enableDebug_(enableDebug),
      instance_(VK_NULL_HANDLE),
      debugMessenger_(VK_NULL_HANDLE),
      surface_(VK_NULL_HANDLE),
      physicalDevice_(VK_NULL_HANDLE) {
}

Device::~Device() {
    destroy();
}

void Device::destroy() {
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (enableDebug_ && debugMessenger_ != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                        instance_, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance_, debugMessenger_, nullptr);
            debugMessenger_ = VK_NULL_HANDLE;
        }
    }

    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

bool Device::prepare(void* windowNativeHandle) {
    LOGI("Prepare Device(Vulkan) ...");

    if (!createInstance()) return false;
    if (!setupValidationLayer()) return false;
    if (!createSurface(windowNativeHandle)) return false;
    if (!createPhysicalDevice()) return false;
    if (!createLogicalDevice()) return false;

    return true;
}

bool Device::getLastError(std::string* pError) {
    if (last_error_.empty()) {
        return false;
    }

    if (pError != nullptr) {
        *pError = last_error_;
    }
    return true;
}

void Device::onValidationLayerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      flag,
    VkDebugUtilsMessageTypeFlagsEXT             typeFlag,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData
) {
    if (flag & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        LOGD("Validation Layer msg: " << pCallbackData->pMessage);
    } else if (flag & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        LOGI("Validation Layer msg: " << pCallbackData->pMessage);
    } else if (flag & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOGW("Validation Layer msg: " << pCallbackData->pMessage);
    } else if (flag & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        LOGE("Validation Layer msg: " << pCallbackData->pMessage);
        last_error_ = pCallbackData->pMessage;
    }
}

bool Device::createInstance() {
    LOGD("Create Instance ...");

    // require required extensions from glFW
    auto extensions = GetRequiredExtensions(enableDebug_);

    VkApplicationInfo appInfo {
        .sType       = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion  = VK_API_VERSION_1_0,
    };

    VkInstanceCreateInfo info {
        .sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount   = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    // enable debug messenger for vkCreateInstance
    VkDebugUtilsMessengerCreateInfoEXT dbgInfo = {};

    if (enableDebug_) {
        AppendDebugMessengerCreateInfo(&dbgInfo, this);

        info.enabledLayerCount   = to_uint32(REQUIRED_LAYERS.size());
        info.ppEnabledLayerNames = REQUIRED_LAYERS.data();
        info.pNext               = &dbgInfo;
    }

    VkResult result = vkCreateInstance(&info, nullptr, &this->instance_);
    if (result != VK_SUCCESS) {
        LOGE("Failed to create instance. result: " << string_VkResult(result));
        return false;
    }

    return true;
}

bool Device::setupValidationLayer() {
    if (enableDebug_ == false) {
        return true;
    }

    LOGD("Setup Debug Messenger ...");

    VkDebugUtilsMessengerCreateInfoEXT dbgInfo = {};
    AppendDebugMessengerCreateInfo(&dbgInfo, this);

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
                    instance_, "vkCreateDebugUtilsMessengerEXT");
    if (func == nullptr) {
        LOGE("Extension vkCreateDebugUtilsMessengerEXT not support");
        return false;
    }

    return func(instance_, &dbgInfo, nullptr, &debugMessenger_) == VK_SUCCESS;
}

bool Device::createSurface(void* windowNativeHandle) {
    LOGD("Create surface ...");

    if (windowNativeHandle == nullptr) {
        LOGE("Invalid window ptr : nullptr");
        return false;
    }

    auto pWinHandle = reinterpret_cast<GLFWwindow*>(windowNativeHandle);
    VkResult result = glfwCreateWindowSurface(
                        instance_, pWinHandle, nullptr, &surface_);
    if (result != VK_SUCCESS) {
        LOGE("Failed to create window surface. result: " << string_VkResult(result));
        return false;
    }
    return true;
}

bool Device::createPhysicalDevice() {
    LOGD("Create physical device ...");

    VkResult result;
    uint32_t deviceCount = 0;
    result = vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

    if (result != VK_SUCCESS || deviceCount == 0) {
        LOGE("Failed to find the valid physicaldevice supported, result: " << string_VkResult(result));
        return false;
    }

    std::string errorMsg;
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (checkDeviceSuitable(device, surface_, &errorMsg)) {
            physicalDevice_ = device;
            break;
        }
    }

    if (physicalDevice_ == VK_NULL_HANDLE) {
        LOGE("Failed to find a suitable physical device.");
        return false;
    }

    // get the device properties.
    vkGetPhysicalDeviceProperties(
        physicalDevice_,
        &physicalDeviceProperties_);

    vkGetPhysicalDeviceMemoryProperties(
        physicalDevice_,
        &physicalDeviceMemProperties_);

    LOGD("Select physical device[" << physicalDeviceProperties_.deviceID
          << "]:" << physicalDeviceProperties_.deviceName);

    return true;
}

bool Device::createLogicalDevice() {
    LOGD("Create logical device ...");

    if (!QuaryGraphicQueueFamilies(physicalDevice_,
                                   surface_,
                                   &queueFamilyIndices_)) {
        LOGE("Fail to query graphics queue families");
        return false;
    }

    std::vector<VkDeviceQueueCreateInfo> createInfos;
    std::set<uint32_t> uniqueQueueFamilies = {graphicsQueueFamily(),
                                              presentQueueFamily()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        createInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures {
        .samplerAnisotropy = VK_TRUE,
    };

    VkDeviceCreateInfo info {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount    = to_uint32(createInfos.size()),
        .pQueueCreateInfos       = createInfos.data(),
        .enabledExtensionCount   = to_uint32(REQUIRED_DEVICE_EXTENSIONS.size()),
        .ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS.data(),
        .pEnabledFeatures        = &deviceFeatures,
    };

    if (enableDebug_) {
        info.enabledLayerCount   = to_uint32(REQUIRED_LAYERS.size());
        info.ppEnabledLayerNames = REQUIRED_LAYERS.data();
    } else {
        info.enabledLayerCount = 0;
    }

    VkResult result = vkCreateDevice(physicalDevice_, &info, nullptr, &device_);

    if (result != VK_SUCCESS) {
        LOGE("Failed to create logical device. result:" << string_VkResult(result));
        return false;
    }

    // Query queues from device
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    vkGetDeviceQueue(device_, graphicsQueueFamily(), 0, &graphicsQueue);
    vkGetDeviceQueue(device_, presentQueueFamily(), 0, &presentQueue);

    graphicsQueue_ = std::make_unique<cgl::vk::Queue>(
        cgl::IQueue::Type::GraphicsQueue,
        this->device(),
        graphicsQueue);

    presentQueue_ = std::make_unique<cgl::vk::Queue>(
        cgl::IQueue::Type::PresentQueue,
        this->device(),
        presentQueue);

    return true;
}

cgl::Results Device::waitIdle()  {
    if (device_ == VK_NULL_HANDLE) {
        LOGE("Invalid device.");
        return cgl::Results::InvalidRendererDevice;
    }

    VkResult result = vkDeviceWaitIdle(device_);
    if (result != VK_SUCCESS) {
        LOGE("Failed to call the vkDeviceWaitIdle. result: "<< string_VkResult(result));
        return cgl::Results::InvalidRendererDevice;
    }

    return cgl::Results::Success;
}

VkResult Device::findMemoryTypeIndex(
    uint32_t              typeFilter,
    VkMemoryPropertyFlags properties,
    uint32_t*             pIndex
) const {
    const auto& prop = physicalDeviceMemProperties_;
    for (uint32_t i = 0; i < prop.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (prop.memoryTypes[i].propertyFlags & properties) == properties) {
            *pIndex = i;
            return VK_SUCCESS;
        }
    }

    return VK_ERROR_FORMAT_NOT_SUPPORTED;
}


bool Device::createBuffer(
    const VkDeviceSize          size,
    const VkBufferUsageFlags    usage,
    const VkMemoryPropertyFlags properties,
    VkBuffer*                   pBuffer,
    VkDeviceMemory*             pBufferMemory
) {
    // Create buffer
    VkBufferCreateInfo bufferInfo {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = size,
        .usage       = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkCreateBuffer(device_, &bufferInfo, nullptr, pBuffer),
        "Failed to create buffer!");

    // Get buffer memory requirements
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device_, *pBuffer, &req);

    VkMemoryAllocateInfo allocInfo {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = req.size,
        .memoryTypeIndex = 0
    };

    // Alocate memory for the buffer
    RETURN_FAIL_IF_ANY_VK_FAILED(
        findMemoryTypeIndex(req.memoryTypeBits, properties,
                            &allocInfo.memoryTypeIndex),
        "No valid memory type index found : memoryTypeBits:"
        << req.memoryTypeBits);

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkAllocateMemory(device_, &allocInfo, nullptr, pBufferMemory),
        "Failed to allocate buffer memory!");

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkBindBufferMemory(device_, *pBuffer, *pBufferMemory, 0),
        "Failed to bind buffer memory");

    return true;
}

bool Device::create2DImage(
    const uint32_t              width,
    const uint32_t              height,
    const VkFormat              format,
    const uint32_t              mipLevels,
    const VkImageTiling         tiling,
    const VkImageUsageFlags     usage,
    const VkMemoryPropertyFlags properties,
    VkImage*                    pImage,
    VkDeviceMemory*             pImageMemory
) {
    LOGD("Create 2D image resource " << width << "x" << height
         << ", format " << string_VkFormat(format)
         << ", mipLevels " << mipLevels
         << ", tiling {}, " << string_VkImageTiling(tiling)
         << ", usage " << string_VkImageUsageFlags(usage));

    // check input
    if ((pImage == nullptr) || (pImageMemory == nullptr)) {
        LOGE("Invalid image view : nullptr");
    }

    VkExtent3D extent {
        .width  = width,
        .height = height,
        .depth  = 1
    };

    VkImageCreateInfo imageInfo {
        .sType          = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType      = VK_IMAGE_TYPE_2D,
        .format         = format,
        .extent         = extent,
        .mipLevels      = mipLevels,
        .arrayLayers    = 1,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .tiling         = tiling,
        .usage          = usage,
        .sharingMode    = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED};

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkCreateImage(device_, &imageInfo, nullptr, pImage),
        "Failed to create image");

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device(), *pImage, &req);

    VkMemoryAllocateInfo allocInfo {
        .sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size};

    RETURN_FAIL_IF_ANY_VK_FAILED(
        findMemoryTypeIndex(req.memoryTypeBits, properties,
                            &allocInfo.memoryTypeIndex),
        "No valid memory type index found : memoryTypeBits:"
        << req.memoryTypeBits);

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkAllocateMemory(device_, &allocInfo, nullptr, pImageMemory),
        "Failed to allocate image memory!");

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkBindImageMemory(device_, *pImage, *pImageMemory, 0),
        "Failed to bind memory to image");

    return true;
}

bool Device::create2DImageView(
    const VkImage            image,
    const VkFormat           format,
    const uint32_t           mipLevels,
    const VkImageAspectFlags aspectFlags,
    VkImageView*             pImageView
) {
    // check input
    if (pImageView == nullptr) {
        LOGE("Invalid image view : nullptr");
    }

    VkImageSubresourceRange  subresourceRange {
        .aspectMask     = aspectFlags,
        .baseMipLevel   = 0,
        .levelCount     = mipLevels,
        .baseArrayLayer = 0,
        .layerCount     = 1,
    };

    VkImageViewCreateInfo viewInfo {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = image,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = format,
        .subresourceRange = subresourceRange
    };

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkCreateImageView(device_, &viewInfo, nullptr, pImageView),
        "Failed to create image view");

    return true;
}

bool Device::createShaderModule(
    const std::filesystem::path& filePath,
    VkShaderModule*              pModule
) {
    // check file exist or not
    if ((std::filesystem::exists(filePath) == false) ||
        (std::filesystem::is_regular_file(filePath) == false)) {
        LOGE("Failed create shared module from file, it doesn't exist or not a "
             "file:" << filePath.string());
        return false;
    }

    // Open the file in binary mode
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOGE("Failed to open the file: " << filePath.string());
        return false;
    }

    // Get the size of the file
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read the file into a vector
    std::vector<char> buffer(fileSize);
    if (!file.read(buffer.data(), fileSize)) {
        LOGE("Failed to read the file: " << filePath.string());
        return false;
    }

    VkShaderModuleCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = buffer.size(),
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data()),
    };

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkCreateShaderModule(this->device(), &createInfo, nullptr, pModule),
        "Failed to create shader module from file:" << filePath.string());

    return true;
}
