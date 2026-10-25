#include "cgl/graphics/sprite_renderer.h"
#include "cgl/trace/logger.h"
#include "vulkan/device.h"
#include "vulkan/command_buffer_list.h"
#include "vulkan/render_pass.h"
#include "vulkan/sprite_pipeline.h"
#include "vulkan/common.h"
#include <cstring>
#include <limits>
#include <cmath>

namespace cgl::graphics::vulkan {
namespace {
constexpr uint32_t vertexCode[] =
#include "sprite_vert.inc"
;
constexpr uint32_t fragmentCode[] =
#include "sprite_frag.inc"
;

} // namespace
SpritePipeline::~SpritePipeline() {
    if (pipeline_) vkDestroyPipeline(device_, pipeline_, nullptr);
    if (layout_) vkDestroyPipelineLayout(device_, layout_, nullptr);
    if (descriptorLayout_) vkDestroyDescriptorSetLayout(device_, descriptorLayout_, nullptr);
    if (sampler_) vkDestroySampler(device_, sampler_, nullptr);
}
bool SpritePipeline::prepare(VkRenderPass pass) {
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    RETURN_FAIL_IF_ANY_VK_FAILED(vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_), "Sprite sampler creation failed");

    VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo descriptorInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    descriptorInfo.bindingCount = 1; descriptorInfo.pBindings = &binding;
    RETURN_FAIL_IF_ANY_VK_FAILED(vkCreateDescriptorSetLayout(device_, &descriptorInfo, nullptr, &descriptorLayout_), "Sprite descriptor layout failed");
    VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 36};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1; layoutInfo.pSetLayouts = &descriptorLayout_;
    layoutInfo.pushConstantRangeCount = 1; layoutInfo.pPushConstantRanges = &push;
    RETURN_FAIL_IF_ANY_VK_FAILED(vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &layout_), "Sprite pipeline layout failed");
    VkShaderModule vertex = VK_NULL_HANDLE, fragment = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shader.codeSize = sizeof(vertexCode); shader.pCode = vertexCode;
    RETURN_FAIL_IF_ANY_VK_FAILED(vkCreateShaderModule(device_, &shader, nullptr, &vertex), "Sprite vertex shader failed");
    shader.codeSize = sizeof(fragmentCode); shader.pCode = fragmentCode;
    const auto fragmentResult = vkCreateShaderModule(device_, &shader, nullptr, &fragment);
    if (fragmentResult != VK_SUCCESS) {
        vkDestroyShaderModule(device_, vertex, nullptr);
        LOGE("Sprite fragment shader creation failed");
        return false;
    }
    VkPipelineShaderStageCreateInfo stages[2]{};
    for (auto& stage : stages) { stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stage.pName = "main"; }
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vertex;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fragment;
    VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL; raster.cullMode = VK_CULL_MODE_NONE; raster.lineWidth = 1;
    VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.blendEnable = VK_TRUE;
    attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    attachment.colorBlendOp = attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1; blend.pAttachments = &attachment;
    VkDynamicState dynamics[]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = 2; dynamic.pDynamicStates = dynamics;
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    info.stageCount = 2; info.pStages = stages; info.pVertexInputState = &input;
    info.pInputAssemblyState = &assembly; info.pViewportState = &viewport; info.pRasterizationState = &raster;
    info.pMultisampleState = &samples; info.pColorBlendState = &blend; info.pDynamicState = &dynamic;
    info.layout = layout_; info.renderPass = pass;
    const auto pipelineResult = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline_);
    vkDestroyShaderModule(device_, fragment, nullptr);
    vkDestroyShaderModule(device_, vertex, nullptr);
    RETURN_FAIL_IF_ANY_VK_FAILED(pipelineResult, "Sprite graphics pipeline failed");
    LOGI("Shared sprite pipeline created");
    return true;
}

std::shared_ptr<SpritePipeline> IRenderPass::acquireSpritePipeline() {
    // Render-thread only, as with sprite creation and queue submission.
    if (!spritePipeline_) {
        auto candidate = std::make_shared<SpritePipeline>(device());
        if (!candidate->prepare(renderPass())) return nullptr;
        spritePipeline_ = std::move(candidate);
    }
    return spritePipeline_;
}
namespace {
class SpriteRenderer final : public ISpriteRenderer {
public:
    explicit SpriteRenderer(Device* device) : device_(device->device()), physical_(device->physicalDevice()),
        queueFamily_(device->graphicsQueueFamily()) {}
    ~SpriteRenderer() override {
        // Cancellation only waits for this renderer's submitted copy, not queue idle.
        if (submitted_) vkWaitForFences(device_,1,&uploadFence_,VK_TRUE,UINT64_MAX);
        releaseUpload();
        if (descriptorPool_) vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        if (view_) vkDestroyImageView(device_, view_, nullptr);
        if (image_) vkDestroyImage(device_, image_, nullptr);
        if (imageMemory_) vkFreeMemory(device_, imageMemory_, nullptr);
    }

    bool prepare(IRenderPass* pass, uint32_t width, uint32_t height,
                 uint32_t frameCount, std::span<const uint8_t> rgba);
    bool preparePending(IRenderPass* pass,uint32_t width,uint32_t height,uint32_t frames,
                        std::vector<uint8_t> rgba,size_t chunkBytes) {
        deferred_=true; chunkBytes_=chunkBytes; pixels_=std::move(rgba);
        return prepare(pass,width,height,frames,pixels_);
    }
    SpriteUploadState pollUpload() override;
    void draw(ICommandBuffer* command, const cgl::Viewport& viewport,
              uint32_t frame, float x, float y, float scale) override;
    void drawRegion(ICommandBuffer* command, const cgl::Viewport& viewport,
                    uint32_t frame, SpriteRegion source, float x, float y, float scale) override;
private:
    bool allocate(const VkMemoryRequirements& requirements, VkMemoryPropertyFlags flags, VkDeviceMemory* memory) {
        VkPhysicalDeviceMemoryProperties properties;
        vkGetPhysicalDeviceMemoryProperties(physical_, &properties);
        for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
            if ((requirements.memoryTypeBits & (1u << i)) &&
                (properties.memoryTypes[i].propertyFlags & flags) == flags) {
                VkMemoryAllocateInfo info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
                info.allocationSize = requirements.size;
                info.memoryTypeIndex = i;
                RETURN_FAIL_IF_ANY_VK_FAILED(vkAllocateMemory(device_, &info, nullptr, memory), "Sprite memory allocation failed");
                return true;
            }
        }
        LOGE("No suitable sprite memory type");
        return false;
    }
    bool upload(std::span<const uint8_t> rgba);
    bool submitChunk(std::span<const uint8_t> rgba);
    void releaseUpload() noexcept;
    bool deferred_=false, submitted_=false;
    SpriteUploadState uploadState_=SpriteUploadState::Pending;
    size_t chunkBytes_=0, uploadedRows_=0;
    std::vector<uint8_t> pixels_;
    VkFence uploadFence_=VK_NULL_HANDLE;
    VkCommandBuffer uploadCommand_=VK_NULL_HANDLE;

    std::shared_ptr<SpritePipeline> pipeline_;
    VkDevice device_;
    VkPhysicalDevice physical_;
    uint32_t queueFamily_, width_ = 0, height_ = 0, frameCount_ = 0;
    VkImage image_ = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory_ = VK_NULL_HANDLE;
    VkImageView view_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_ = VK_NULL_HANDLE;
    VkBuffer staging_ = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory_ = VK_NULL_HANDLE;
    VkCommandPool uploadPool_ = VK_NULL_HANDLE;
};

bool SpriteRenderer::prepare(IRenderPass* pass, uint32_t width, uint32_t height,
                             uint32_t frameCount, std::span<const uint8_t> rgba) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_, &properties);
    const uint64_t pixels = uint64_t(width) * height;
    if (!width || !height || !frameCount ||
        width > properties.limits.maxImageDimension2D || height > properties.limits.maxImageDimension2D ||
        frameCount > properties.limits.maxImageArrayLayers ||
        pixels > std::numeric_limits<size_t>::max() / 4 / frameCount ||
        rgba.size() != pixels * 4 * frameCount) {
        LOGE("Invalid sprite texture dimensions or byte count");
        return false;
    }
    if(deferred_ && chunkBytes_<size_t(width)*4) return false;
    width_ = width; height_ = height; frameCount_ = frameCount;
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = frameCount;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    RETURN_FAIL_IF_ANY_VK_FAILED(vkCreateImage(device_, &imageInfo, nullptr, &image_), "Sprite image creation failed");
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device_, image_, &requirements);
    if (!allocate(requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &imageMemory_)) return false;
    RETURN_FAIL_IF_ANY_VK_FAILED(vkBindImageMemory(device_, image_, imageMemory_, 0), "Sprite image binding failed");
    if (!upload(rgba)) return false;

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = image_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = imageInfo.format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, frameCount};
    RETURN_FAIL_IF_ANY_VK_FAILED(vkCreateImageView(device_, &viewInfo, nullptr, &view_), "Sprite view creation failed");
    pipeline_ = pass->acquireSpritePipeline();
    if (!pipeline_) return false;
    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 1; poolInfo.poolSizeCount = 1; poolInfo.pPoolSizes = &poolSize;
    RETURN_FAIL_IF_ANY_VK_FAILED(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_), "Sprite descriptor pool failed");
    VkDescriptorSetAllocateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    setInfo.descriptorPool = descriptorPool_; setInfo.descriptorSetCount = 1; setInfo.pSetLayouts = &pipeline_->descriptorLayout_;
    RETURN_FAIL_IF_ANY_VK_FAILED(vkAllocateDescriptorSets(device_, &setInfo, &descriptor_), "Sprite descriptor allocation failed");
    VkDescriptorImageInfo imageDescriptor{pipeline_->sampler_, view_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = descriptor_; write.dstBinding = 0; write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.pImageInfo = &imageDescriptor;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    return true;
}

void SpriteRenderer::releaseUpload() noexcept {
    if (uploadPool_) vkDestroyCommandPool(device_,uploadPool_,nullptr);
    if (uploadFence_) vkDestroyFence(device_,uploadFence_,nullptr);
    if (staging_) vkDestroyBuffer(device_,staging_,nullptr);
    if (stagingMemory_) vkFreeMemory(device_,stagingMemory_,nullptr);
    uploadPool_=VK_NULL_HANDLE;uploadFence_=VK_NULL_HANDLE;staging_=VK_NULL_HANDLE;stagingMemory_=VK_NULL_HANDLE;
    std::vector<uint8_t>().swap(pixels_);
}
bool SpriteRenderer::upload(std::span<const uint8_t> rgba) {
    if(!deferred_) chunkBytes_=rgba.size();
    VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer.size=std::min(chunkBytes_,rgba.size());buffer.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    RETURN_FAIL_IF_ANY_VK_FAILED(vkCreateBuffer(device_,&buffer,nullptr,&staging_),"Sprite staging buffer failed");
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device_,staging_,&requirements);
    if(!allocate(requirements,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&stagingMemory_)) return false;
    RETURN_FAIL_IF_ANY_VK_FAILED(vkBindBufferMemory(device_,staging_,stagingMemory_,0),"Sprite staging binding failed");
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pool.queueFamilyIndex=queueFamily_;
    RETURN_FAIL_IF_ANY_VK_FAILED(vkCreateCommandPool(device_,&pool,nullptr,&uploadPool_),"Sprite upload pool failed");
    VkCommandBufferAllocateInfo command{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    command.commandPool=uploadPool_;command.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;command.commandBufferCount=1;
    RETURN_FAIL_IF_ANY_VK_FAILED(vkAllocateCommandBuffers(device_,&command,&uploadCommand_),"Sprite upload command failed");
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    RETURN_FAIL_IF_ANY_VK_FAILED(vkCreateFence(device_,&fence,nullptr,&uploadFence_),"Sprite upload fence failed");
    if(!submitChunk(rgba)) return false;
    if(!deferred_) {
        RETURN_FAIL_IF_ANY_VK_FAILED(vkWaitForFences(device_,1,&uploadFence_,VK_TRUE,UINT64_MAX),"Sprite upload wait failed");
        submitted_=false;uploadState_=SpriteUploadState::Ready;releaseUpload();
    }
    return true;
}
bool SpriteRenderer::submitChunk(std::span<const uint8_t> rgba) {
    const size_t rowBytes=size_t(width_)*4,totalRows=size_t(height_)*frameCount_;
    const size_t rows=std::min(totalRows-uploadedRows_,chunkBytes_/rowBytes),bytes=rows*rowBytes;
    void* mapped=nullptr;
    RETURN_FAIL_IF_ANY_VK_FAILED(vkMapMemory(device_,stagingMemory_,0,bytes,0,&mapped),"Sprite mapping failed");
    std::memcpy(mapped,rgba.data()+uploadedRows_*rowBytes,bytes);vkUnmapMemory(device_,stagingMemory_);
    RETURN_FAIL_IF_ANY_VK_FAILED(vkResetCommandPool(device_,uploadPool_,0),"Sprite upload pool reset failed");
    RETURN_FAIL_IF_ANY_VK_FAILED(vkResetFences(device_,1,&uploadFence_),"Sprite upload fence reset failed");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    RETURN_FAIL_IF_ANY_VK_FAILED(vkBeginCommandBuffer(uploadCommand_,&begin),"Sprite upload begin failed");
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.image=image_;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,frameCount_};
    if(!uploadedRows_) {
        barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(uploadCommand_,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    }
    std::vector<VkBufferImageCopy> regions;
    for(size_t offset=0;offset<rows;) {
        const size_t row=uploadedRows_+offset,localRow=row%height_;
        const auto count=uint32_t(std::min(rows-offset,height_-localRow));
        VkBufferImageCopy copy{};copy.bufferOffset=offset*rowBytes;
        copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,uint32_t(row/height_),1};
        copy.imageOffset={0,int32_t(localRow),0};copy.imageExtent={width_,count,1};
        regions.push_back(copy);offset+=count;
    }
    vkCmdCopyBufferToImage(uploadCommand_,staging_,image_,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,uint32_t(regions.size()),regions.data());
    if(uploadedRows_+rows==totalRows) {
        barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(uploadCommand_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    }
    RETURN_FAIL_IF_ANY_VK_FAILED(vkEndCommandBuffer(uploadCommand_),"Sprite upload end failed");
    VkQueue queue;vkGetDeviceQueue(device_,queueFamily_,0,&queue);
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&uploadCommand_;
    RETURN_FAIL_IF_ANY_VK_FAILED(vkQueueSubmit(queue,1,&submit,uploadFence_),"Sprite upload submission failed");
    submitted_=true;uploadedRows_+=rows;
    return true;
}
SpriteUploadState SpriteRenderer::pollUpload() {
    if(uploadState_!=SpriteUploadState::Pending) return uploadState_;
    const auto status=vkGetFenceStatus(device_,uploadFence_);
    if(status==VK_NOT_READY) return SpriteUploadState::Pending;
    if(status!=VK_SUCCESS) {uploadState_=SpriteUploadState::Failed;return uploadState_;}
    submitted_=false;
    if(uploadedRows_==size_t(height_)*frameCount_) {
        uploadState_=SpriteUploadState::Ready;releaseUpload();
    } else if(!submitChunk(pixels_)) uploadState_=SpriteUploadState::Failed;
    return uploadState_;
}


void SpriteRenderer::draw(ICommandBuffer* command, const cgl::Viewport& viewport,
                           uint32_t frame, float x, float y, float scale) {
    drawRegion(command, viewport, frame, {0, 0, width_, height_}, x, y, scale);
}

void SpriteRenderer::drawRegion(ICommandBuffer* command, const cgl::Viewport& viewport,
                                uint32_t frame, SpriteRegion source, float x, float y, float scale) {
    if (uploadState_!=SpriteUploadState::Ready || !command || frame >= frameCount_ || !viewport.width || !viewport.height ||
        !source.width || !source.height || source.x >= width_ || source.y >= height_ ||
        source.width > width_ - source.x || source.height > height_ - source.y ||
        viewport.x < 0 || viewport.y < 0 || !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(scale) || scale <= 0) return;
    const float push[]{2 * x / viewport.width - 1, 2 * y / viewport.height - 1,
                       2 * source.width * scale / viewport.width, 2 * source.height * scale / viewport.height,
                       float(source.x) / width_, float(source.y) / height_,
                       float(source.width) / width_, float(source.height) / height_, float(frame)};
    // Finite inputs can still overflow during normalization or at the far
    // corner added by the vertex shader. Ignore before recording any state.
    if (!std::isfinite(push[0]) || !std::isfinite(push[1]) ||
        !std::isfinite(push[2]) || !std::isfinite(push[3]) ||
        !std::isfinite(push[0] + push[2]) || !std::isfinite(push[1] + push[3])) return;
    static_cast<CommandBuffer*>(command)->drawSprite(pipeline_->pipeline_,pipeline_->layout_,descriptor_,viewport,push);
}
}
}

cgl::graphics::ISpriteRenderer::Ptr cgl::graphics::ISpriteRenderer::create(
    IDevice* device, IRenderPass* pass, uint32_t width, uint32_t height,
    uint32_t frameCount, std::span<const uint8_t> rgba) {
    auto* nativeDevice = dynamic_cast<vulkan::Device*>(device);
    auto* nativePass = dynamic_cast<vulkan::IRenderPass*>(pass);
    if (!nativeDevice || !nativePass || !nativePass->renderPass() ||
        nativeDevice->device() != nativePass->device()) return nullptr;
    auto renderer = std::make_unique<vulkan::SpriteRenderer>(nativeDevice);
    if (!renderer->prepare(nativePass, width, height, frameCount, rgba)) return nullptr;
    return renderer;
}

cgl::graphics::ISpriteRenderer::Ptr cgl::graphics::ISpriteRenderer::createPending(
    IDevice* device,IRenderPass* pass,uint32_t width,uint32_t height,uint32_t frameCount,
    std::vector<uint8_t> rgba,size_t chunkBytes) {
    auto* nativeDevice=dynamic_cast<vulkan::Device*>(device);
    auto* nativePass=dynamic_cast<vulkan::IRenderPass*>(pass);
    if(!nativeDevice || !nativePass || !nativePass->renderPass() || nativeDevice->device()!=nativePass->device()) return nullptr;
    auto renderer=std::make_unique<vulkan::SpriteRenderer>(nativeDevice);
    if(!renderer->preparePending(nativePass,width,height,frameCount,std::move(rgba),chunkBytes)) return nullptr;
    return renderer;
}
