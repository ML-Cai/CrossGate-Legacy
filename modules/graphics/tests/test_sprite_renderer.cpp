#include <gtest/gtest.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <array>
#include <cstring>
#include <limits>
#include <functional>
#include <chrono>
#include <thread>
#include "cgl/graphics/sprite_renderer.h"
#include "vulkan/device.h"
#include "vulkan/command_buffer_list.h"
#include "vulkan/framebuffer.h"
#include "vulkan/render_pass.h"

namespace {
namespace graphics = cgl::graphics;
namespace vk = cgl::graphics::vulkan;
constexpr uint32_t side = 8;
using Pixel = std::array<uint8_t, 4>;
using Pixels = std::array<Pixel, side * side>;

// Test-only color pass: unlike a swapchain, its result can be copied to the CPU.
class OffscreenPass final : public vk::IRenderPass {
public:
    OffscreenPass(VkDevice device, VkRenderPass pass) : IRenderPass(device) { renderPass_ = pass; }
    ~OffscreenPass() override { vkDestroyRenderPass(device(), renderPass_, nullptr); }
    bool begin(const cgl::Viewport& area, graphics::ICommandBuffer* command,
               graphics::IFramebuffer* framebuffer) override {
        command_ = static_cast<vk::CommandBuffer*>(command)->commandBuffer();
        VkClearValue clear{};
        clear.color.float32[2] = clear.color.float32[3] = 1; // Opaque blue background.
        VkRenderPassBeginInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        info.renderPass = renderPass_;
        info.framebuffer = static_cast<vk::Framebuffer*>(framebuffer)->buffer();
        info.renderArea = {{area.x, area.y}, {area.width, area.height}};
        info.clearValueCount = 1; info.pClearValues = &clear;
        vkCmdBeginRenderPass(command_, &info, VK_SUBPASS_CONTENTS_INLINE);
        return true;
    }
    bool end() override { vkCmdEndRenderPass(command_); return true; }
    graphics::IFramebuffer* acquireFramebuffer(uint32_t) const override { return nullptr; }
private:
    VkCommandBuffer command_ = VK_NULL_HANDLE;
};

class SpriteRendererTest : public testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(glfwInit(), GLFW_TRUE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        window_ = glfwCreateWindow(side, side, "sprite test", nullptr, nullptr);
        ASSERT_NE(window_, nullptr);
        device_ = graphics::IDevice::create(window_, true);
        ASSERT_NE(device_, nullptr);
        native_ = static_cast<vk::Device*>(device_.get());
        handle_ = native_->device();
        device_->getLastError(&initialDiagnostic_); // The loader may report stale local layer manifests.

        VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        image.imageType = VK_IMAGE_TYPE_2D; image.format = VK_FORMAT_R8G8B8A8_UNORM;
        image.extent = {side, side, 1}; image.mipLevels = image.arrayLayers = 1;
        image.samples = VK_SAMPLE_COUNT_1_BIT; image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ASSERT_EQ(vkCreateImage(handle_, &image, nullptr, &image_), VK_SUCCESS);
        VkMemoryRequirements requirements;
        vkGetImageMemoryRequirements(handle_, image_, &requirements);
        ASSERT_EQ(allocate(requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &imageMemory_), VK_SUCCESS);
        ASSERT_EQ(vkBindImageMemory(handle_, image_, imageMemory_, 0), VK_SUCCESS);
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image = image_; view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.format = image.format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        ASSERT_EQ(vkCreateImageView(handle_, &view, nullptr, &view_), VK_SUCCESS);

        VkAttachmentDescription attachment{};
        attachment.format = image.format; attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &color;
        VkSubpassDependency dependency{};
        dependency.srcSubpass = 0; dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dependency.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        VkRenderPassCreateInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        pass.attachmentCount = 1; pass.pAttachments = &attachment;
        pass.subpassCount = 1; pass.pSubpasses = &subpass;
        pass.dependencyCount = 1; pass.pDependencies = &dependency;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        ASSERT_EQ(vkCreateRenderPass(handle_, &pass, nullptr, &renderPass), VK_SUCCESS);
        pass_ = std::make_unique<OffscreenPass>(handle_, renderPass);
        VkFramebufferCreateInfo framebuffer{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebuffer.renderPass = renderPass; framebuffer.attachmentCount = 1; framebuffer.pAttachments = &view_;
        framebuffer.width = framebuffer.height = side; framebuffer.layers = 1;
        VkFramebuffer target = VK_NULL_HANDLE;
        ASSERT_EQ(vkCreateFramebuffer(handle_, &framebuffer, nullptr, &target), VK_SUCCESS);
        framebuffer_ = std::make_unique<vk::Framebuffer>(handle_);
        framebuffer_->setBuffer(target);

        VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buffer.size = sizeof(Pixels); buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        ASSERT_EQ(vkCreateBuffer(handle_, &buffer, nullptr, &readback_), VK_SUCCESS);
        vkGetBufferMemoryRequirements(handle_, readback_, &requirements);
        ASSERT_EQ(allocate(requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           &readbackMemory_), VK_SUCCESS);
        ASSERT_EQ(vkBindBufferMemory(handle_, readback_, readbackMemory_, 0), VK_SUCCESS);
        commands_ = graphics::ICommandBufferList::create(device_.get(), 1);
        ASSERT_NE(commands_, nullptr);
    }
    void TearDown() override {
        if (device_) device_->waitIdle();
        renderer_.reset();
        commands_.reset();
        framebuffer_.reset();
        pass_.reset();
        if (readback_) vkDestroyBuffer(handle_, readback_, nullptr);
        if (readbackMemory_) vkFreeMemory(handle_, readbackMemory_, nullptr);
        if (view_) vkDestroyImageView(handle_, view_, nullptr);
        if (image_) vkDestroyImage(handle_, image_, nullptr);
        if (imageMemory_) vkFreeMemory(handle_, imageMemory_, nullptr);
        if (device_) {
            std::string diagnostic;
            device_->getLastError(&diagnostic);
            EXPECT_EQ(diagnostic, initialDiagnostic_) << "New Vulkan validation error";
        }
        device_.reset();
        if (window_) glfwDestroyWindow(window_);
        glfwTerminate();
    }
    VkResult allocate(const VkMemoryRequirements& requirements, VkMemoryPropertyFlags flags, VkDeviceMemory* memory) {
        const auto properties = native_->physicalDeviceMemoryProperties();
        for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
            if ((requirements.memoryTypeBits & (1u << i)) &&
                (properties.memoryTypes[i].propertyFlags & flags) == flags) {
                VkMemoryAllocateInfo info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
                info.allocationSize = requirements.size; info.memoryTypeIndex = i;
                return vkAllocateMemory(handle_, &info, nullptr, memory);
            }
        }
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }
    void renderDraw(const std::function<void(graphics::ICommandBuffer*, const cgl::Viewport&)>& draw, Pixels* result) {
        auto* command = commands_->commandBuffer(0);
        ASSERT_TRUE(command->reset());
        ASSERT_TRUE(command->begin());
        const cgl::Viewport area(0, 0, side, side);
        ASSERT_TRUE(pass_->begin(area, command, framebuffer_.get()));
        draw(command, area);
        ASSERT_TRUE(pass_->end());
        const auto raw = static_cast<vk::CommandBuffer*>(command)->commandBuffer();
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}; copy.imageExtent = {side, side, 1};
        vkCmdCopyImageToBuffer(raw, image_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback_, 1, &copy);
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(raw, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                             0, 1, &barrier, 0, nullptr, 0, nullptr);
        ASSERT_TRUE(command->end());
        VkQueue queue;
        vkGetDeviceQueue(handle_, native_->graphicsQueueFamily(), 0, &queue);
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1; submit.pCommandBuffers = &raw;
        ASSERT_EQ(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE), VK_SUCCESS);
        ASSERT_EQ(vkQueueWaitIdle(queue), VK_SUCCESS);
        void* mapped = nullptr;
        ASSERT_EQ(vkMapMemory(handle_, readbackMemory_, 0, sizeof(Pixels), 0, &mapped), VK_SUCCESS);
        std::memcpy(result->data(), mapped, sizeof(Pixels));
        vkUnmapMemory(handle_, readbackMemory_);
    }
    void render(uint32_t frame, Pixels* result, float x = 2, float y = 2, float scale = 2) {
        renderDraw([&](auto* command, const auto& area) { renderer_->draw(command, area, frame, x, y, scale); }, result);
    }
    GLFWwindow* window_ = nullptr;
    graphics::IDevice::Ptr device_;
    vk::Device* native_ = nullptr;
    VkDevice handle_ = VK_NULL_HANDLE;
    VkImage image_ = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory_ = VK_NULL_HANDLE, readbackMemory_ = VK_NULL_HANDLE;
    VkImageView view_ = VK_NULL_HANDLE;
    VkBuffer readback_ = VK_NULL_HANDLE;
    std::unique_ptr<OffscreenPass> pass_;
    std::unique_ptr<vk::Framebuffer> framebuffer_;
    graphics::ICommandBufferList::Ptr commands_;
    graphics::ISpriteRenderer::Ptr renderer_;
    std::string initialDiagnostic_;
};

// Top row: opaque red, transparent green. Bottom: half-alpha red, opaque white.
constexpr std::array<uint8_t, 32> texture{
    255,0,0,255, 0,255,0,0, 255,0,0,128, 255,255,255,255,
    0,255,0,255, 0,255,0,255, 0,255,0,255, 0,255,0,255
};

TEST_F(SpriteRendererTest, RgbaOrientationTransparencyAndNearestScaling) {
    renderer_ = graphics::ISpriteRenderer::create(device_.get(), pass_.get(), 2, 2, 2, texture);
    ASSERT_NE(renderer_, nullptr);
    Pixels pixels{};
    render(0, &pixels);
    for (uint32_t y = 0; y < side; ++y) for (uint32_t x = 0; x < side; ++x) {
        Pixel expected{0,0,255,255};
        if (x >= 2 && x < 6 && y >= 2 && y < 6) {
            if (y < 4 && x < 4) expected = {255,0,0,255};
            else if (y >= 4 && x < 4) expected = {128,0,127,255};
            else if (y >= 4) expected = {255,255,255,255};
        }
        const auto actual = pixels[y * side + x];
        for (int channel = 0; channel < 4; ++channel)
            EXPECT_NEAR(actual[channel], expected[channel], 1) << "Pixel " << x << "," << y << ", channel " << channel;
    }
}

TEST_F(SpriteRendererTest, FrameSelectionAndInvalidDraws) {
    renderer_ = graphics::ISpriteRenderer::create(device_.get(), pass_.get(), 2, 2, 2, texture);
    ASSERT_NE(renderer_, nullptr);
    Pixels pixels{};
    render(1, &pixels, 1, 1, 1);
    EXPECT_EQ(pixels[1 * side + 1], (Pixel{0,255,0,255}));
    EXPECT_EQ(pixels[2 * side + 2], (Pixel{0,255,0,255}));
    EXPECT_EQ(pixels[3 * side + 3], (Pixel{0,0,255,255}));
    for (auto frame : {2u, std::numeric_limits<uint32_t>::max()}) {
        render(frame, &pixels);
        for (const auto& pixel : pixels) EXPECT_EQ(pixel, (Pixel{0,0,255,255}));
    }
    render(0, &pixels, 0, 0, std::numeric_limits<float>::quiet_NaN());
    for (const auto& pixel : pixels) EXPECT_EQ(pixel, (Pixel{0,0,255,255}));
}

TEST_F(SpriteRendererTest, RejectsInvalidTextureData) {
    EXPECT_EQ(graphics::ISpriteRenderer::create(nullptr, pass_.get(), 2, 2, 2, texture), nullptr);
    EXPECT_EQ(graphics::ISpriteRenderer::create(device_.get(), nullptr, 2, 2, 2, texture), nullptr);
    EXPECT_EQ(graphics::ISpriteRenderer::create(device_.get(), pass_.get(), 0, 2, 2, texture), nullptr);
    EXPECT_EQ(graphics::ISpriteRenderer::create(device_.get(), pass_.get(), 2, 2, 0, texture), nullptr);
    EXPECT_EQ(graphics::ISpriteRenderer::create(device_.get(), pass_.get(), 2, 2, 3, texture), nullptr);
    EXPECT_EQ(graphics::ISpriteRenderer::create(device_.get(), pass_.get(), UINT32_MAX, 2, 2, texture), nullptr);
}
TEST_F(SpriteRendererTest, AtlasRegionsUseLocalSizeAndRejectOutOfBoundsSources) {
    renderer_ = graphics::ISpriteRenderer::create(device_.get(), pass_.get(), 2, 2, 2, texture);
    ASSERT_NE(renderer_, nullptr);
    Pixels pixels{};
    renderDraw([&](auto* command, const auto& area) {
        renderer_->drawRegion(command, area, 0, {1, 1, 1, 1}, 2, 2, 2);
    }, &pixels);
    for (uint32_t y = 0; y < side; ++y) for (uint32_t x = 0; x < side; ++x)
        EXPECT_EQ(pixels[y * side + x], (x >= 2 && x < 4 && y >= 2 && y < 4 ?
            Pixel{255,255,255,255} : Pixel{0,0,255,255}));
    for (const graphics::SpriteRegion source : {graphics::SpriteRegion{0,0,0,1}, {0,0,1,0},
            {2,0,1,1}, {0,2,1,1}, {1,1,2,1}, {UINT32_MAX,0,1,1}, {1,0,UINT32_MAX,1}}) {
        renderDraw([&](auto* command, const auto& area) {
            renderer_->drawRegion(command, area, 0, source, 0, 0, 1);
        }, &pixels);
        for (const auto& pixel : pixels) EXPECT_EQ(pixel, (Pixel{0,0,255,255}));
    }
}

TEST_F(SpriteRendererTest, PainterOrderPreservesObjectsBehindTransparentPixels) {
    renderer_ = graphics::ISpriteRenderer::create(device_.get(), pass_.get(), 2, 2, 2, texture);
    ASSERT_NE(renderer_, nullptr);
    Pixels pixels{};
    renderDraw([&](auto* command, const auto& area) {
        renderer_->draw(command, area, 1, 2, 2, 2); // Green object behind.
        renderer_->draw(command, area, 0, 2, 2, 2); // Red/white foreground with a hole.
    }, &pixels);
    EXPECT_EQ(pixels[2 * side + 2], (Pixel{255,0,0,255}));
    EXPECT_EQ(pixels[2 * side + 4], (Pixel{0,255,0,255}));
    EXPECT_NEAR(pixels[4 * side + 2][0], 128, 1);
    EXPECT_NEAR(pixels[4 * side + 2][1], 127, 1);
    EXPECT_EQ(pixels[4 * side + 4], (Pixel{255,255,255,255}));
    renderDraw([&](auto* command, const auto& area) {
        renderer_->draw(command, area, 0, 2, 2, 2);
        renderer_->draw(command, area, 1, 2, 2, 2); // Now green is entirely in front.
    }, &pixels);
    for (uint32_t y = 2; y < 6; ++y) for (uint32_t x = 2; x < 6; ++x)
        EXPECT_EQ(pixels[y * side + x], (Pixel{0,255,0,255}));
}

}

TEST_F(SpriteRendererTest, AlternatingAtlasRenderersPreservePainterOrderAndTransparency) {
    renderer_ = graphics::ISpriteRenderer::create(device_.get(),pass_.get(),2,2,2,texture);
    auto other = graphics::ISpriteRenderer::create(device_.get(),pass_.get(),2,2,2,texture);
    ASSERT_NE(renderer_,nullptr); ASSERT_NE(other,nullptr);
    Pixels pixels{};
    renderDraw([&](auto* command,const auto& area) {
        renderer_->drawRegion(command,area,0,{0,0,1,1},0,0,4); // Page A red.
        other->drawRegion(command,area,1,{0,0,1,1},1,1,4); // Page B green.
        renderer_->drawRegion(command,area,0,{1,0,1,1},2,2,2); // A transparent.
        renderer_->drawRegion(command,area,0,{1,1,1,1},3,3,2); // A white on top.
    },&pixels);
    EXPECT_EQ(pixels[0],(Pixel{255,0,0,255}));
    EXPECT_EQ(pixels[side+1],(Pixel{0,255,0,255}));
    EXPECT_EQ(pixels[2*side+2],(Pixel{0,255,0,255}));
    EXPECT_EQ(pixels[3*side+3],(Pixel{255,255,255,255}));
    EXPECT_EQ(pixels[5*side+5],(Pixel{0,0,255,255}));
}

TEST_F(SpriteRendererTest, SharedPipelineSurvivesTextureChurnAndReleasesWithLastOwner) {
    auto* pass=static_cast<vk::IRenderPass*>(pass_.get());
    auto shared=pass->acquireSpritePipeline();
    ASSERT_NE(shared,nullptr);
    std::weak_ptr<vk::SpritePipeline> weak=shared;
    for(int i=0;i<8;++i) {
        renderer_=graphics::ISpriteRenderer::create(device_.get(),pass_.get(),2,2,2,texture);
        ASSERT_NE(renderer_,nullptr);
        EXPECT_EQ(pass->acquireSpritePipeline(),shared);
        Pixels pixels{};
        renderDraw([&](auto* command,const auto& area){renderer_->draw(command,area,0,0,0,1);},&pixels);
        EXPECT_EQ(pixels[0],(Pixel{255,0,0,255}));
        device_->waitIdle();
        renderer_.reset();
    }
    renderer_=graphics::ISpriteRenderer::create(device_.get(),pass_.get(),2,2,2,texture);
    shared.reset();
    device_->waitIdle();
    framebuffer_.reset(); pass_.reset();
    EXPECT_FALSE(weak.expired()); // Texture still holds the immutable bundle.
    renderer_.reset();
    EXPECT_TRUE(weak.expired()); // No global/static cache retains a device resource.
}

TEST_F(SpriteRendererTest, PendingUploadSplitsRowsAndLayersAndHidesUntilComplete) {
    renderer_=graphics::ISpriteRenderer::createPending(device_.get(),pass_.get(),2,2,2,
        std::vector<uint8_t>(texture.begin(),texture.end()),8); // One RGBA row per chunk.
    ASSERT_NE(renderer_,nullptr);
    Pixels pixels{};
    render(0,&pixels);
    EXPECT_EQ(pixels[2*side+2],(Pixel{0,0,255,255})); // Pending draw is a no-op.
    auto state=graphics::SpriteUploadState::Pending;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(state==graphics::SpriteUploadState::Pending && std::chrono::steady_clock::now()<deadline) {
        state=renderer_->pollUpload();std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(state,graphics::SpriteUploadState::Ready);
    EXPECT_EQ(renderer_->pollUpload(),state);
    render(0,&pixels);
    EXPECT_EQ(pixels[2*side+2],(Pixel{255,0,0,255}));
    EXPECT_EQ(pixels[5*side+5],(Pixel{255,255,255,255}));
    render(1,&pixels);
    EXPECT_EQ(pixels[2*side+2],(Pixel{0,255,0,255}));
}
TEST_F(SpriteRendererTest, PendingCancellationWaitsForOwnCopyAndRejectsTooSmallChunk) {
    EXPECT_EQ(graphics::ISpriteRenderer::createPending(device_.get(),pass_.get(),2,2,2,
        std::vector<uint8_t>(texture.begin(),texture.end()),7),nullptr);
    for(int i=0;i<8;++i) {
        auto pending=graphics::ISpriteRenderer::createPending(device_.get(),pass_.get(),2,2,2,
            std::vector<uint8_t>(texture.begin(),texture.end()),8);
        ASSERT_NE(pending,nullptr); // Drop while first chunk may still be submitted.
    }
}
