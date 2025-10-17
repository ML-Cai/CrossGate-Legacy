// -----------------------------------------------------------------------------
//   The MIT License (MIT)
//
//   Copyright (c) 2024 MengLun,Cai
//
//   All rights reserved.
//------------------------------------------------------------------------------

#include "cgl/render/device.h"
#include "cgl/trace/logger.h"

#include "vulkan/device.h"
#include "vulkan/swapchain.h"
#include "vulkan/command_buffer_list.h"
#include "vulkan/framebuffer.h"
#include "vulkan/common.h"
#include "vulkan/render_pass/init_scene_render_pass_impl.h"

using cgl::vk::Device;
using cgl::vk::InitSceneRenderPass;

// -----------------------------------------------------------------------------
namespace {

const VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

}   // namespace

// -----------------------------------------------------------------------------
// cgl::vk::InitSceneRenderPass
// -----------------------------------------------------------------------------
InitSceneRenderPass::InitSceneRenderPass(VkDevice device)
    : cgl::vk::IRenderPass(device),
      pLastICmdBuffer_(nullptr) {
}

InitSceneRenderPass::~InitSceneRenderPass() {
    destroy();
}

void InitSceneRenderPass::destroy() {
    CGL_SAFE_DESTROY_RENDER_PASS(this->device(), renderPass_);
}

bool InitSceneRenderPass::prepare(cgl::ISwapchain* pSwapchain) {
    LOGI("Prepare InitSceneRenderPass ...");

    if (pSwapchain == nullptr) {
        return false;
    }

    if (!createRenderPass(pSwapchain)) return false;
    if (!createFramebuffer(pSwapchain)) return false;

    return true;
}

bool InitSceneRenderPass::createRenderPass(cgl::ISwapchain* pSwapchain) {
    LOGD("Create InitSceneRenderPass ...");

    auto pVkSwapchain = static_cast<cgl::vk::Swapchain *>(pSwapchain);

    VkAttachmentDescription colorAttachment {
        .format         = pVkSwapchain->imageFormat(),
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference colorAttachmentRef {
        .attachment = COLOR_ATTACTMENT_IDX,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &colorAttachmentRef,
    };

    VkSubpassDependency dependency {
        .srcSubpass    = VK_SUBPASS_EXTERNAL,
        .dstSubpass    = 0,
        .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkAttachmentDescription attachments[] = {colorAttachment};
    VkRenderPassCreateInfo renderPassInfo {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = CGL_ARRAY_COUNT(attachments),
        .pAttachments    = attachments,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 1,
        .pDependencies   = &dependency,
    };

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkCreateRenderPass(this->device(),
                           &renderPassInfo,
                           nullptr,
                           &renderPass_),
        "Failed to create render pass for InitScene");

    return true;
}

bool InitSceneRenderPass::createFramebuffer(cgl::ISwapchain* pSwapchain) {
    bool allBufferReady = false;
    auto pVkSwapchain = static_cast<cgl::vk::Swapchain *>(pSwapchain);

    for (uint32_t i = 0; i < pVkSwapchain->imageCount() ; i++) {
        VkImageView attachments[] = {
            pVkSwapchain->swapChainImageView(i)
        };

        VkFramebufferCreateInfo framebufferInfo {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = renderPass_,
            .attachmentCount = CGL_ARRAY_COUNT(attachments),
            .pAttachments    = attachments,
            .width           = pVkSwapchain->extent().width,
            .height          = pVkSwapchain->extent().height,
            .layers          = 1,
        };

        // create vulkan buffer
        VkFramebuffer buffer;
        BREAK_IF_ANY_VK_FAILED(
            vkCreateFramebuffer(this->device(),
                                &framebufferInfo,
                                nullptr,
                                &buffer),
            "Failed to create the framebuffer");

        // create cgl buffer object and assign VkFramebuffer to it
        auto pFramebuffer = std::make_unique<cgl::vk::Framebuffer>(device());
        if (pFramebuffer == nullptr) {
            LOGE("Failed to create framebuffer("
                 << framebufferInfo.width << "x" << framebufferInfo.height
                 << ") for render pass");
            break;
        }

        auto pFramebufferImpl = reinterpret_cast<cgl::vk::Framebuffer *>(
                                    pFramebuffer.get());
        pFramebufferImpl->setBuffer(buffer);
        frameBufferList_.emplace_back(std::move(pFramebuffer));
        allBufferReady = true;
    }

    // check if all framebuffer create successfully, or release all allocated
    // buffer.
    if (!allBufferReady) {
        for (auto& buf : frameBufferList_) {
            auto pFramebufferImpl = reinterpret_cast<cgl::vk::Framebuffer *>(
                                    buf.get());
            vkDestroyFramebuffer(this->device(),
                                 pFramebufferImpl->buffer(),
                                 nullptr);
        }
    }

    return allBufferReady;
}

bool InitSceneRenderPass::begin(
    const cgl::Viewport& viewport,
    cgl::ICommandBuffer* pICmdBuffer,
    cgl::IFramebuffer*   pIFramebuffer
) {
    // check input
    if ((pICmdBuffer == nullptr) || (pIFramebuffer == nullptr)) {
        LOGE("Invalid input args");
        return false;
    }

    auto pFrameBuffer = static_cast<cgl::vk::Framebuffer*>(pIFramebuffer);
    VkClearValue clearValue {
        .color = {0.0f, 0.0f, 0.0f, 1.0f}
    };

    VkRenderPassBeginInfo renderPassInfo {
        .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass  = renderPass_,
        .framebuffer = pFrameBuffer->buffer(),
        .renderArea  = VkRect2D{
            .offset = VkOffset2D {viewport.x, viewport.y},
            .extent = VkExtent2D {viewport.width, viewport.height}
        },
        .clearValueCount = 1,
        .pClearValues    = &clearValue,
    };

    auto pVkCmdBuffer = static_cast<cgl::vk::CommandBuffer*>(pICmdBuffer);
    vkCmdBeginRenderPass(pVkCmdBuffer->commandBuffer(), &renderPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    pLastICmdBuffer_ = pICmdBuffer;

    return true;
}

bool InitSceneRenderPass::end() {
    if (pLastICmdBuffer_ == nullptr) {
        LOGE("No command buffer begined for this render pass");
        return false;
    }

    auto pCmdBuffer = static_cast<cgl::vk::CommandBuffer*>(pLastICmdBuffer_);
    vkCmdEndRenderPass(pCmdBuffer->commandBuffer());

    return true;
}
