// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>
#include <string_view>

#define RETURN_FAIL_IF_ANY_VK_FAILED(vkcondition, ...) \
    do { \
        VkResult result = vkcondition; \
        if (result != VK_SUCCESS) { \
            LOGE(__VA_ARGS__); \
            LOGE("    ... Error code:" << string_VkResult(result)); \
            return false; \
        } \
    } while (false);

#define RETURN_FALSE_IF_ERROR(condition, ...) \
    do { \
        if (condition == false) { \
            LOGE(__VA_ARGS__); \
            return false; \
        } \
    } while (false);

#define BREAK_IF_ANY_VK_FAILED(vkcondition, ...) \
    VkResult result_ ## __LINE__ = vkcondition; \
    if (result_ ## __LINE__ != VK_SUCCESS) { \
        LOGE(__VA_ARGS__); \
        LOGE("    ... Error code: " << string_VkResult(result_ ## __LINE__)); \
        break; \
    }

#define CGL_SAFE_DESTROY_IMAGEVIEW(device, obj) \
    if (obj != VK_NULL_HANDLE) { \
        vkDestroyImageView(device, obj, nullptr); \
        obj = VK_NULL_HANDLE; \
    }

#define CGL_SAFE_DESTROY_IMAGE(device, obj) \
    if (obj != VK_NULL_HANDLE) { \
        vkDestroyImage(device, obj, nullptr); \
        obj = VK_NULL_HANDLE; \
    }

#define CGL_SAFE_DESTROY_SAMPLER(device, obj) \
    if (obj != VK_NULL_HANDLE) { \
        vkDestroySampler(device, obj, nullptr); \
        obj = VK_NULL_HANDLE; \
    }

#define CGL_SAFE_FREE_MEMORY(device, obj) \
    if (obj != VK_NULL_HANDLE) { \
        vkFreeMemory(device, obj, nullptr); \
        obj = VK_NULL_HANDLE; \
    }

#define CGL_SAFE_FREE_BUFFER(device, obj) \
    if (obj != VK_NULL_HANDLE) { \
        vkDestroyBuffer(device, obj, nullptr); \
        obj = VK_NULL_HANDLE; \
    }

#define CGL_SAFE_DESTROY_RENDER_PASS(device, obj) \
    if (obj != VK_NULL_HANDLE) { \
        vkDestroyRenderPass(device, obj, nullptr); \
        obj = VK_NULL_HANDLE; \
    }

#define CGL_SAFE_DESTROY_PIPELINE(device, obj) \
    if (obj != VK_NULL_HANDLE) { \
        vkDestroyPipeline(device, obj, nullptr); \
        obj = VK_NULL_HANDLE; \
    }

#define CGL_SAFE_DESTROY_PIPELINE_LAYOUT(device, obj) \
    if (obj != VK_NULL_HANDLE) { \
        vkDestroyPipelineLayout(device, obj, nullptr); \
        obj = VK_NULL_HANDLE; \
    }

#define CGL_SAFE_DESTROY_DESCRIPTOR_SET_LAYOUT(device, obj) \
    if (obj != VK_NULL_HANDLE) { \
        vkDestroyDescriptorSetLayout(device, obj, nullptr); \
        obj = VK_NULL_HANDLE; \
    }

#define CGL_SAFE_DESTROY_DESCRIPTOR_POOL(device, obj) \
    if (obj != VK_NULL_HANDLE) { \
        vkDestroyDescriptorPool(device, obj, nullptr); \
        obj = VK_NULL_HANDLE; \
    }

#define CGL_ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

constexpr std::string_view SHADER_BASE_PATH = "shaders/vulkan/";

#define DEFINE_FILE_SHADER(renderer_name, shader_file_name) \
    std::filesystem::path(SHADER_BASE_PATH) / \
    std::filesystem::path(renderer_name) / \
    std::filesystem::path(shader_file_name)
