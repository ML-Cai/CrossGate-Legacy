#include <gtest/gtest.h>
#include "vulkan/device.h"

TEST(DeviceValidationDiagnostics, CountsRepeatedValidationErrorsButNotLoaderMessagesOrWarnings) {
    cgl::graphics::vulkan::Device device(false); // No window or Vulkan initialization.
    VkDebugUtilsMessengerCallbackDataEXT data{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT};
    data.pMessage="Synthetic diagnostic for counter test";
    device.onValidationLayerCallback(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,&data);
    device.onValidationLayerCallback(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,&data);
    EXPECT_EQ(device.validationErrorCount(),0u);
    for(int i=0;i<2;++i) device.onValidationLayerCallback(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,&data);
    EXPECT_EQ(device.validationErrorCount(),2u);
    device.onValidationLayerCallback(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,&data);
    EXPECT_EQ(device.validationErrorCount(),3u);
    device.destroy();device.destroy(); // Safe before prepare and on repeated cleanup.
    EXPECT_EQ(device.validationErrorCount(),3u); // Survives teardown assertions.
}
