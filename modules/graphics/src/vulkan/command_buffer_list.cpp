// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "cgl/graphics/device.h"
#include "cgl/trace/logger.h"
#include "vulkan/device.h"
#include "vulkan/command_buffer_list.h"
#include "vulkan/common.h"

using cgl::graphics::vulkan::CommandBufferList;
using cgl::graphics::vulkan::CommandBuffer;

// -----------------------------------------------------------------------------
// cgl::graphics::vulkan::CommandBufferList
// -----------------------------------------------------------------------------
CommandBufferList::CommandBufferList(
    cgl::graphics::IDevice* pDevice,
    uint32_t                bufferCount)
    : cmdPool_(VK_NULL_HANDLE) {
    auto pNativeDevice   = reinterpret_cast<cgl::graphics::vulkan::Device *>(pDevice);
    device_              = pNativeDevice->device();
    graphicsQueueFamily_ = pNativeDevice->graphicsQueueFamily();
    cmdBuffers_.resize(bufferCount);
    uint32_t count=0;vkGetPhysicalDeviceQueueFamilyProperties(pNativeDevice->physicalDevice(),&count,nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(pNativeDevice->physicalDevice(),&count,families.data());
    if(graphicsQueueFamily_<count) clock_={families[graphicsQueueFamily_].timestampValidBits,
        pNativeDevice->physicalDeviceProperties().limits.timestampPeriod};
}

CommandBufferList::~CommandBufferList() {
    destroy();
}

void CommandBufferList::destroy() {
    // Same completion requirement as destroying the command pool itself.
    for(auto& buffer:cmdBuffers_) if(buffer) static_cast<CommandBuffer*>(buffer.get())->destroy();
    if (cmdPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, cmdPool_, nullptr);
        cmdPool_ = VK_NULL_HANDLE;
    }
}

bool CommandBufferList::prepare() {
    LOGI("Prepare CommandBufferList with " << bufferCount() << " buffers.");

    if (!createCommandPool()) return false;
    if (!createCommandBuffers()) return false;

    return true;
}

bool CommandBufferList::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsQueueFamily_,
    };

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkCreateCommandPool(device_, &poolInfo, nullptr, &cmdPool_),
        "Failed to create command pool");

    return true;
}

bool CommandBufferList::createCommandBuffers() {
    for (size_t i = 0 ; i < cmdBuffers_.size() ; i++) {
        auto cmdBuffer = std::make_unique<cgl::graphics::vulkan::CommandBuffer>(
                            device_, cmdPool_, static_cast<uint32_t>(i),clock_);

        if (!cmdBuffer->prepare()) {
            LOGE("Failed to create command buffer, index:" << i);
            return false;
        }
        cmdBuffers_[i] = std::move(cmdBuffer);
    }

    return true;
}


// -----------------------------------------------------------------------------
// cgl::graphics::vulkan::CommandBuffer
// -----------------------------------------------------------------------------
CommandBuffer::CommandBuffer(VkDevice device, VkCommandPool pool, uint32_t idx,TimestampClock clock)
    : device_(device),
      pool_(pool),
      cmdBuffer_(VK_NULL_HANDLE),
      idx_(idx),clock_(clock) {
}

CommandBuffer::~CommandBuffer() {destroy();}

bool CommandBuffer::reset() {
    invalidateGraphicsState();spriteBindings_={};timingRecorded_=false;
    auto result = vkResetCommandBuffer(cmdBuffer_, 0);
    if (result != VK_SUCCESS) {
        LOGE("Failed to reset the command buffer.");
        return false;
    }

    return true;
}

bool CommandBuffer::begin() {
    invalidateGraphicsState();spriteBindings_={};timingRecorded_=false;
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    auto result = vkBeginCommandBuffer(cmdBuffer_, &beginInfo);
    if (result != VK_SUCCESS) {
        LOGE("Failed to begin the command buffer.");
        return false;
    }

    if(timestampPool_) {
        vkCmdResetQueryPool(cmdBuffer_,timestampPool_,0,2);
        vkCmdWriteTimestamp(cmdBuffer_,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,timestampPool_,0);
    }
    return true;
}

bool CommandBuffer::end() {
    invalidateGraphicsState();timingRecorded_=false;
    if(timestampPool_) vkCmdWriteTimestamp(cmdBuffer_,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,timestampPool_,1);
    auto result = vkEndCommandBuffer(cmdBuffer_);
    if (result != VK_SUCCESS) {
        LOGE("Failed to end the command buffer.");
        return false;
    }

    timingRecorded_=bool(timestampPool_);return true;
}

void CommandBuffer::destroy() {
    invalidateGraphicsState();spriteBindings_={};timingRecorded_=false;
    if(timestampPool_) {vkDestroyQueryPool(device_,timestampPool_,nullptr);timestampPool_=VK_NULL_HANDLE;}
    cmdBuffer_ = VK_NULL_HANDLE;
}

bool CommandBuffer::prepare() {
    LOGD("Prepare CommandBuffer(Vulkan), index: " << idx());

    return createCommandBuffer();
}

bool CommandBuffer::enableGpuTiming() {
    if(timestampPool_) return true;
    if(!clock_.supported() || !cmdBuffer_) return false;
    const VkQueryPoolCreateInfo info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,nullptr,0,VK_QUERY_TYPE_TIMESTAMP,2,0};
    if(vkCreateQueryPool(device_,&info,nullptr,&timestampPool_)!=VK_SUCCESS) {
        timestampPool_=VK_NULL_HANDLE;return false;
    }
    return true;
}
std::optional<double> CommandBuffer::takeGpuTimeAfterCompletion() {
    if(!timestampPool_ || !timingRecorded_) return std::nullopt;
    timingRecorded_=false;
    struct Query {uint64_t value,available;};Query results[2]{};
    const auto result=vkGetQueryPoolResults(device_,timestampPool_,0,2,sizeof(results),results,sizeof(Query),
        VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
    if(result!=VK_SUCCESS || !results[0].available || !results[1].available) return std::nullopt;
    return clock_.milliseconds(results[0].value,results[1].value);
}

void CommandBuffer::drawSprite(VkPipeline pipeline,VkPipelineLayout layout,VkDescriptorSet descriptor,const cgl::Viewport& viewport,const float (&push)[9]) {
    bindSpriteState(pipeline,layout,descriptor,viewport);
    vkCmdPushConstants(cmdBuffer_,layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),push);
    vkCmdDraw(cmdBuffer_,4,1,0,0);
}

void CommandBuffer::bindSpriteState(VkPipeline pipeline,VkPipelineLayout layout,VkDescriptorSet descriptor,const cgl::Viewport& viewport) {
    const auto changes=spriteState_.update(pipeline,layout,descriptor,viewport);
    if(changes.viewport) {
        const VkViewport area{float(viewport.x),float(viewport.y),float(viewport.width),float(viewport.height),0,1};
        const VkRect2D scissor{{viewport.x,viewport.y},{viewport.width,viewport.height}};
        vkCmdSetViewport(cmdBuffer_,0,1,&area);vkCmdSetScissor(cmdBuffer_,0,1,&scissor);++spriteBindings_.viewports;
    }
    if(changes.pipeline) {
        vkCmdBindPipeline(cmdBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);++spriteBindings_.pipelines;
    }
    if(changes.descriptor) {
        vkCmdBindDescriptorSets(cmdBuffer_,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,1,&descriptor,0,nullptr);++spriteBindings_.descriptors;
    }
}

bool CommandBuffer::createCommandBuffer() {
    VkCommandBufferAllocateInfo allocInfo {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = pool_,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkAllocateCommandBuffers(device_, &allocInfo, &cmdBuffer_),
        "Failed to create command buffers");

    return true;
}
