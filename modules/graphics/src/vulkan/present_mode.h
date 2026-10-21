#pragma once
#include <algorithm>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace cgl::graphics::vulkan {
inline VkPresentModeKHR SelectPresentMode(const std::vector<VkPresentModeKHR>& modes, bool vsync) {
    if (!vsync) {
        for (auto preferred : {VK_PRESENT_MODE_IMMEDIATE_KHR,VK_PRESENT_MODE_MAILBOX_KHR})
            if (std::find(modes.begin(),modes.end(),preferred)!=modes.end()) return preferred;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}
}
