#include <gtest/gtest.h>
#include "vulkan/present_mode.h"
using cgl::graphics::vulkan::SelectPresentMode;
TEST(PresentModeTest, VsyncAlwaysRequestsFifo) {
    EXPECT_EQ(SelectPresentMode({VK_PRESENT_MODE_IMMEDIATE_KHR,VK_PRESENT_MODE_FIFO_KHR,VK_PRESENT_MODE_MAILBOX_KHR},true),VK_PRESENT_MODE_FIFO_KHR);
}
TEST(PresentModeTest, UncappedPrefersImmediateOverMailbox) {
    EXPECT_EQ(SelectPresentMode({VK_PRESENT_MODE_MAILBOX_KHR,VK_PRESENT_MODE_FIFO_KHR,VK_PRESENT_MODE_IMMEDIATE_KHR},false),VK_PRESENT_MODE_IMMEDIATE_KHR);
}
TEST(PresentModeTest, ReportsSynchronizedFallbackModes) {
    EXPECT_EQ(SelectPresentMode({VK_PRESENT_MODE_MAILBOX_KHR,VK_PRESENT_MODE_FIFO_KHR},false),VK_PRESENT_MODE_MAILBOX_KHR);
    EXPECT_EQ(SelectPresentMode({VK_PRESENT_MODE_FIFO_KHR},false),VK_PRESENT_MODE_FIFO_KHR);
}
