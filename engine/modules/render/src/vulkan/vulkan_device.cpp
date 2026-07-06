// Vulkan backend for the RHI: instance → surface → device → swapchain → render
// pass, with GPU buffers, SPIR-V graphics pipelines, and per-frame command
// recording so callers draw their own geometry. See docs/adr/0006.
#include "vulkan/vulkan_device.hpp"

#include <zukiru/platform/window.hpp>

#if ZUKIRU_RENDER_VK_XLIB
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#if ZUKIRU_RENDER_VK_WAYLAND
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif
#include <vulkan/vulkan.h>

// vulkan_xlib.h drags in Xlib, which #defines lowercase identifiers we use.
#ifdef None
#undef None
#endif

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace zukiru::render {
namespace {

constexpr u32 kFramesInFlight = 2;

#define ZK_VK(expr)                                                                \
    do {                                                                           \
        const VkResult zkRes = (expr);                                             \
        if (zkRes != VK_SUCCESS) {                                                 \
            error_ = std::string(#expr) + " -> VkResult " + std::to_string(zkRes); \
            return false;                                                          \
        }                                                                          \
    } while (false)

[[nodiscard]] VkFormat toVkFormat(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float32: return VK_FORMAT_R32_SFLOAT;
        case VertexFormat::Float32x2: return VK_FORMAT_R32G32_SFLOAT;
        case VertexFormat::Float32x3: return VK_FORMAT_R32G32B32_SFLOAT;
        case VertexFormat::Float32x4: return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
    return VK_FORMAT_R32G32B32_SFLOAT;
}

[[nodiscard]] VkPrimitiveTopology toVkTopology(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PrimitiveTopology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

struct VulkanBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
};

struct VulkanPipeline {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;  // VK_NULL_HANDLE if no bindings
};

struct VulkanTexture {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
};

struct VulkanBindGroup {
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;  // the pipeline layout it was built for
};

struct VulkanRenderTarget {
    u32 colorTextureId = 0;  // the sampleable color attachment, stored in textures_
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory = VK_NULL_HANDLE;
    VkImageView depthView = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkExtent2D extent{};
    Color clearColor{};
};

[[nodiscard]] VkDescriptorType toVkDescriptorType(BindingType type) {
    return type == BindingType::UniformBuffer ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                              : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
}

class VulkanDevice final : public Device {
public:
    VulkanDevice() = default;
    ~VulkanDevice() override { teardown(); }

    [[nodiscard]] const std::string& error() const noexcept { return error_; }

    bool init(const platform::Window& window, const DeviceConfig& config) {
        config_ = config;
        window_ = &window;
        const platform::WindowExtent extent = window.extent();
        width_ = extent.width;
        height_ = extent.height;

        if (!createInstance()) return false;
        if (!createSurface(window)) return false;
        if (!pickPhysicalDevice()) return false;
        if (!createLogicalDevice()) return false;
        if (!createSwapchain()) return false;
        if (!createDepthResources()) return false;
        if (!createRenderPass()) return false;
        if (!createFramebuffers()) return false;
        if (!createCommandResources()) return false;
        if (!createSyncObjects()) return false;
        if (!createDescriptorPool()) return false;
        return true;
    }

    // --- Resources -------------------------------------------------------

    BufferHandle createBuffer(BufferKind kind, const void* data, usize sizeBytes) override {
        if (sizeBytes == 0) return {};

        VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        switch (kind) {
            case BufferKind::Vertex: usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; break;
            case BufferKind::Index: usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT; break;
            case BufferKind::Uniform: usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; break;
        }

        VulkanBuffer resource;
        if (!allocateBuffer(sizeBytes, usage,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            resource.buffer, resource.memory)) {
            return {};
        }
        resource.size = sizeBytes;

        if (data != nullptr) {
            void* mapped = nullptr;
            vkMapMemory(device_, resource.memory, 0, sizeBytes, 0, &mapped);
            std::memcpy(mapped, data, sizeBytes);
            vkUnmapMemory(device_, resource.memory);
        }

        const u32 id = nextBufferId_++;
        buffers_[id] = resource;
        return BufferHandle{id};
    }

    void destroyBuffer(BufferHandle handle) override {
        const auto it = buffers_.find(handle.id);
        if (it == buffers_.end()) return;
        vkDestroyBuffer(device_, it->second.buffer, nullptr);
        vkFreeMemory(device_, it->second.memory, nullptr);
        buffers_.erase(it);
    }

    void updateBuffer(BufferHandle handle, const void* data, usize sizeBytes) override {
        const auto it = buffers_.find(handle.id);
        if (it == buffers_.end() || data == nullptr) return;
        void* mapped = nullptr;
        vkMapMemory(device_, it->second.memory, 0, sizeBytes, 0, &mapped);
        std::memcpy(mapped, data, sizeBytes);
        vkUnmapMemory(device_, it->second.memory);
    }

    TextureHandle createTexture(u32 width, u32 height, const void* rgbaPixels) override {
        if (width == 0 || height == 0 || rgbaPixels == nullptr) return {};
        const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;

        VkBuffer staging = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        if (!allocateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            staging, stagingMemory)) {
            return {};
        }
        void* mapped = nullptr;
        vkMapMemory(device_, stagingMemory, 0, imageSize, 0, &mapped);
        std::memcpy(mapped, rgbaPixels, imageSize);
        vkUnmapMemory(device_, stagingMemory);

        VulkanTexture texture;
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {width, height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(device_, &imageInfo, nullptr, &texture.image) != VK_SUCCESS) {
            vkDestroyBuffer(device_, staging, nullptr);
            vkFreeMemory(device_, stagingMemory, nullptr);
            return {};
        }

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, texture.image, &requirements);
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = requirements.size;
        alloc.memoryTypeIndex =
            findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(device_, &alloc, nullptr, &texture.memory);
        vkBindImageMemory(device_, texture.image, texture.memory, 0);

        transitionImageLayout(texture.image, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(staging, texture.image, width, height);
        transitionImageLayout(texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(device_, staging, nullptr);
        vkFreeMemory(device_, stagingMemory, nullptr);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = texture.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(device_, &viewInfo, nullptr, &texture.view);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        vkCreateSampler(device_, &samplerInfo, nullptr, &texture.sampler);

        const u32 id = nextTextureId_++;
        textures_[id] = texture;
        return TextureHandle{id};
    }

    void destroyTexture(TextureHandle handle) override {
        const auto it = textures_.find(handle.id);
        if (it == textures_.end()) return;
        vkDestroySampler(device_, it->second.sampler, nullptr);
        vkDestroyImageView(device_, it->second.view, nullptr);
        vkDestroyImage(device_, it->second.image, nullptr);
        vkFreeMemory(device_, it->second.memory, nullptr);
        textures_.erase(it);
    }

    Result<PipelineHandle> createPipeline(const PipelineDesc& desc) override {
        VulkanPipeline pipeline;
        if (!buildPipeline(desc, pipeline)) {
            return Err(Error{error_.empty() ? "render: pipeline creation failed" : error_});
        }
        const u32 id = nextPipelineId_++;
        pipelines_[id] = pipeline;
        return Ok(PipelineHandle{id});
    }

    void destroyPipeline(PipelineHandle handle) override {
        const auto it = pipelines_.find(handle.id);
        if (it == pipelines_.end()) return;
        vkDestroyPipeline(device_, it->second.pipeline, nullptr);
        vkDestroyPipelineLayout(device_, it->second.layout, nullptr);
        if (it->second.setLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_, it->second.setLayout, nullptr);
        }
        pipelines_.erase(it);
    }

    Result<BindGroupHandle> createBindGroup(PipelineHandle pipeline,
                                            std::span<const BindGroupEntry> entries) override {
        const auto pit = pipelines_.find(pipeline.id);
        if (pit == pipelines_.end()) return Err(Error{"createBindGroup: unknown pipeline"});
        if (pit->second.setLayout == VK_NULL_HANDLE) {
            return Err(Error{"createBindGroup: pipeline declares no bindings"});
        }

        VkDescriptorSetAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc.descriptorPool = descriptorPool_;
        alloc.descriptorSetCount = 1;
        alloc.pSetLayouts = &pit->second.setLayout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        if (vkAllocateDescriptorSets(device_, &alloc, &set) != VK_SUCCESS) {
            return Err(Error{"createBindGroup: descriptor pool exhausted"});
        }

        // Stable backing storage for the write infos (reserved so no reallocation
        // invalidates the pointers we hand to Vulkan).
        std::vector<VkDescriptorBufferInfo> bufferInfos;
        std::vector<VkDescriptorImageInfo> imageInfos;
        bufferInfos.reserve(entries.size());
        imageInfos.reserve(entries.size());
        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(entries.size());

        for (const BindGroupEntry& entry : entries) {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = set;
            write.dstBinding = entry.binding;
            write.descriptorCount = 1;

            if (entry.buffer.valid()) {
                const auto bit = buffers_.find(entry.buffer.id);
                if (bit == buffers_.end()) continue;
                VkDescriptorBufferInfo info{};
                info.buffer = bit->second.buffer;
                info.range = VK_WHOLE_SIZE;
                bufferInfos.push_back(info);
                write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                write.pBufferInfo = &bufferInfos.back();
            } else if (entry.texture.valid()) {
                const auto tit = textures_.find(entry.texture.id);
                if (tit == textures_.end()) continue;
                VkDescriptorImageInfo info{};
                info.sampler = tit->second.sampler;
                info.imageView = tit->second.view;
                info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfos.push_back(info);
                write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                write.pImageInfo = &imageInfos.back();
            } else {
                continue;
            }
            writes.push_back(write);
        }
        vkUpdateDescriptorSets(device_, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);

        const u32 id = nextBindGroupId_++;
        bindGroups_[id] = VulkanBindGroup{set, pit->second.layout};
        return Ok(BindGroupHandle{id});
    }

    void destroyBindGroup(BindGroupHandle handle) override {
        const auto it = bindGroups_.find(handle.id);
        if (it == bindGroups_.end()) return;
        vkFreeDescriptorSets(device_, descriptorPool_, 1, &it->second.set);
        bindGroups_.erase(it);
    }

    RenderTargetHandle createRenderTarget(const RenderTargetDesc& desc) override {
        if (desc.width == 0 || desc.height == 0) return {};
        VulkanRenderTarget target;
        target.extent = {desc.width, desc.height};
        target.clearColor = desc.clearColor;

        // Color attachment — swapchain format so any window pipeline renders into it
        // unchanged; SAMPLED so a later pass can read it. Registered as a texture.
        VulkanTexture color;
        if (!allocateImage(desc.width, desc.height, surfaceFormat_.format,
                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                           VK_IMAGE_ASPECT_COLOR_BIT, color.image, color.memory, color.view)) {
            return {};
        }
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        vkCreateSampler(device_, &samplerInfo, nullptr, &color.sampler);
        target.colorTextureId = nextTextureId_++;
        textures_[target.colorTextureId] = color;

        // Depth attachment (not sampled).
        if (!allocateImage(desc.width, desc.height, depthFormat_,
                           VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT,
                           target.depthImage, target.depthMemory, target.depthView)) {
            destroyTextureResources(color);
            textures_.erase(target.colorTextureId);
            return {};
        }

        if (!buildRenderTargetPass(target.renderPass)) {
            destroyRenderTargetResources(target);
            return {};
        }

        const VkImageView attachments[] = {color.view, target.depthView};
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = target.renderPass;
        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments = attachments;
        fbInfo.width = desc.width;
        fbInfo.height = desc.height;
        fbInfo.layers = 1;
        if (vkCreateFramebuffer(device_, &fbInfo, nullptr, &target.framebuffer) != VK_SUCCESS) {
            destroyRenderTargetResources(target);
            return {};
        }

        const u32 id = nextRenderTargetId_++;
        renderTargets_[id] = target;
        return RenderTargetHandle{id};
    }

    void destroyRenderTarget(RenderTargetHandle handle) override {
        const auto it = renderTargets_.find(handle.id);
        if (it == renderTargets_.end()) return;
        destroyRenderTargetResources(it->second);
        renderTargets_.erase(it);
    }

    TextureHandle renderTargetTexture(RenderTargetHandle handle) const override {
        const auto it = renderTargets_.find(handle.id);
        if (it == renderTargets_.end()) return {};
        return TextureHandle{it->second.colorTextureId};
    }

    // --- Frame -----------------------------------------------------------

    bool beginFrame() override {
        vkWaitForFences(device_, 1, &inFlight_[currentFrame_], VK_TRUE, UINT64_MAX);

        const VkResult acquire =
            vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                  imageAvailable_[currentFrame_], VK_NULL_HANDLE, &imageIndex_);
        if (acquire == VK_ERROR_OUT_OF_DATE_KHR) return false;
        if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) return false;

        if (imagesInFlight_[imageIndex_] != VK_NULL_HANDLE) {
            vkWaitForFences(device_, 1, &imagesInFlight_[imageIndex_], VK_TRUE, UINT64_MAX);
        }
        imagesInFlight_[imageIndex_] = inFlight_[currentFrame_];

        VkCommandBuffer cmd = commandBuffers_[currentFrame_];
        vkResetCommandBuffer(cmd, 0);
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cmd, &begin);
        // The command buffer is open but no render pass is — the caller opens one
        // with beginSwapchainPass / beginRenderPass before recording draws.
        return true;
    }

    // --- Passes ----------------------------------------------------------

    void beginSwapchainPass() override {
        VkCommandBuffer cmd = commandBuffers_[currentFrame_];
        VkClearValue clears[2]{};
        clears[0].color = {{clearColor_.r, clearColor_.g, clearColor_.b, clearColor_.a}};
        clears[1].depthStencil = {1.0f, 0};  // farthest depth
        VkRenderPassBeginInfo pass{};
        pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        pass.renderPass = renderPass_;
        pass.framebuffer = framebuffers_[imageIndex_];
        pass.renderArea.extent = extent_;
        pass.clearValueCount = 2;
        pass.pClearValues = clears;
        vkCmdBeginRenderPass(cmd, &pass, VK_SUBPASS_CONTENTS_INLINE);
        setViewportScissor(cmd, extent_);
    }

    void beginRenderPass(RenderTargetHandle handle) override {
        const auto it = renderTargets_.find(handle.id);
        if (it == renderTargets_.end()) return;
        const VulkanRenderTarget& target = it->second;
        VkCommandBuffer cmd = commandBuffers_[currentFrame_];
        VkClearValue clears[2]{};
        clears[0].color = {{target.clearColor.r, target.clearColor.g, target.clearColor.b,
                            target.clearColor.a}};
        clears[1].depthStencil = {1.0f, 0};
        VkRenderPassBeginInfo pass{};
        pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        pass.renderPass = target.renderPass;
        pass.framebuffer = target.framebuffer;
        pass.renderArea.extent = target.extent;
        pass.clearValueCount = 2;
        pass.pClearValues = clears;
        vkCmdBeginRenderPass(cmd, &pass, VK_SUBPASS_CONTENTS_INLINE);
        setViewportScissor(cmd, target.extent);
    }

    void endRenderPass() override {
        vkCmdEndRenderPass(commandBuffers_[currentFrame_]);
    }

    void endFrame() override {
        VkCommandBuffer cmd = commandBuffers_[currentFrame_];
        vkEndCommandBuffer(cmd);

        vkResetFences(device_, 1, &inFlight_[currentFrame_]);

        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &imageAvailable_[currentFrame_];
        submit.pWaitDstStageMask = &waitStage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &renderFinished_[currentFrame_];
        vkQueueSubmit(graphicsQueue_, 1, &submit, inFlight_[currentFrame_]);

        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &renderFinished_[currentFrame_];
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain_;
        present.pImageIndices = &imageIndex_;
        const VkResult result = vkQueuePresentKHR(graphicsQueue_, &present);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            recreateSwapchain();
        }

        currentFrame_ = (currentFrame_ + 1) % kFramesInFlight;
    }

    // --- Recording -------------------------------------------------------

    void bindPipeline(PipelineHandle handle) override {
        const auto it = pipelines_.find(handle.id);
        if (it == pipelines_.end()) return;
        vkCmdBindPipeline(commandBuffers_[currentFrame_], VK_PIPELINE_BIND_POINT_GRAPHICS,
                          it->second.pipeline);
    }

    void bindBindGroup(BindGroupHandle handle) override {
        const auto it = bindGroups_.find(handle.id);
        if (it == bindGroups_.end()) return;
        vkCmdBindDescriptorSets(commandBuffers_[currentFrame_], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                it->second.layout, 0, 1, &it->second.set, 0, nullptr);
    }

    void bindVertexBuffer(BufferHandle handle) override {
        const auto it = buffers_.find(handle.id);
        if (it == buffers_.end()) return;
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffers_[currentFrame_], 0, 1, &it->second.buffer, &offset);
    }

    void bindIndexBuffer(BufferHandle handle, IndexType type) override {
        const auto it = buffers_.find(handle.id);
        if (it == buffers_.end()) return;
        vkCmdBindIndexBuffer(commandBuffers_[currentFrame_], it->second.buffer, 0,
                             type == IndexType::U16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
    }

    void draw(u32 vertexCount, u32 firstVertex) override {
        vkCmdDraw(commandBuffers_[currentFrame_], vertexCount, 1, firstVertex, 0);
    }

    void drawIndexed(u32 indexCount, u32 firstIndex, i32 vertexOffset) override {
        vkCmdDrawIndexed(commandBuffers_[currentFrame_], indexCount, 1, firstIndex, vertexOffset, 0);
    }

    // --- Misc ------------------------------------------------------------

    void setClearColor(Color color) override { clearColor_ = color; }

    void resize(u32 width, u32 height) override {
        width_ = width;
        height_ = height;
        recreateSwapchain();
    }

    void waitIdle() override {
        if (device_ != VK_NULL_HANDLE) vkDeviceWaitIdle(device_);
    }

    [[nodiscard]] Backend backend() const override { return Backend::Vulkan; }
    [[nodiscard]] std::string_view deviceName() const override { return deviceName_; }

private:
    // --- Instance --------------------------------------------------------
    bool createInstance() {
        VkApplicationInfo app{};
        app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app.pApplicationName = "Zukiru";
        app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        app.pEngineName = "Zukiru";
        app.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        app.apiVersion = VK_API_VERSION_1_1;

        std::vector<const char*> extensions{VK_KHR_SURFACE_EXTENSION_NAME};
        switch (window_->nativeBackend()) {
            case platform::NativeBackend::X11:
#if ZUKIRU_RENDER_VK_XLIB
                extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
                break;
            case platform::NativeBackend::Wayland:
#if ZUKIRU_RENDER_VK_WAYLAND
                extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif
                break;
        }

        std::vector<const char*> layers;
        if (config_.enableValidation && validationLayerAvailable()) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
        }

        VkInstanceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        info.pApplicationInfo = &app;
        info.enabledExtensionCount = static_cast<u32>(extensions.size());
        info.ppEnabledExtensionNames = extensions.data();
        info.enabledLayerCount = static_cast<u32>(layers.size());
        info.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();
        ZK_VK(vkCreateInstance(&info, nullptr, &instance_));
        return true;
    }

    static bool validationLayerAvailable() {
        u32 count = 0;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        std::vector<VkLayerProperties> layers(count);
        vkEnumerateInstanceLayerProperties(&count, layers.data());
        for (const VkLayerProperties& layer : layers) {
            if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) return true;
        }
        return false;
    }

    // --- Surface ---------------------------------------------------------
    bool createSurface(const platform::Window& window) {
        switch (window.nativeBackend()) {
#if ZUKIRU_RENDER_VK_XLIB
            case platform::NativeBackend::X11: {
                // Own Display connection (closed before instance destroy) to dodge
                // the NVIDIA XCloseDisplay-hook-into-unloaded-ICD crash.
                Display* display = XOpenDisplay(nullptr);
                if (display == nullptr) {
                    error_ = "render: XOpenDisplay failed for Vulkan surface";
                    return false;
                }
                surfaceDisplayX11_ = display;
                VkXlibSurfaceCreateInfoKHR info{};
                info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
                info.dpy = display;
                info.window = reinterpret_cast<::Window>(window.nativeHandle());
                ZK_VK(vkCreateXlibSurfaceKHR(instance_, &info, nullptr, &surface_));
                return true;
            }
#endif
#if ZUKIRU_RENDER_VK_WAYLAND
            case platform::NativeBackend::Wayland: {
                VkWaylandSurfaceCreateInfoKHR info{};
                info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
                info.display = static_cast<wl_display*>(window.nativeDisplay());
                info.surface = static_cast<wl_surface*>(window.nativeHandle());
                ZK_VK(vkCreateWaylandSurfaceKHR(instance_, &info, nullptr, &surface_));
                return true;
            }
#endif
            default:
                break;
        }
        error_ = "render: window's native backend has no compiled Vulkan surface support";
        return false;
    }

    // --- Physical / logical device ---------------------------------------
    bool pickPhysicalDevice() {
        u32 count = 0;
        vkEnumeratePhysicalDevices(instance_, &count, nullptr);
        if (count == 0) {
            error_ = "render: no Vulkan physical devices";
            return false;
        }
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance_, &count, devices.data());

        VkPhysicalDevice fallback = VK_NULL_HANDLE;
        u32 fallbackFamily = 0;
        for (VkPhysicalDevice candidate : devices) {
            u32 family = 0;
            if (!findGraphicsPresentFamily(candidate, family)) continue;
            if (!supportsSwapchain(candidate)) continue;

            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(candidate, &props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                physicalDevice_ = candidate;
                queueFamily_ = family;
                deviceName_ = props.deviceName;
                return true;
            }
            if (fallback == VK_NULL_HANDLE) {
                fallback = candidate;
                fallbackFamily = family;
                deviceName_ = props.deviceName;
            }
        }
        if (fallback != VK_NULL_HANDLE) {
            physicalDevice_ = fallback;
            queueFamily_ = fallbackFamily;
            return true;
        }
        error_ = "render: no Vulkan device with graphics+present+swapchain";
        return false;
    }

    bool findGraphicsPresentFamily(VkPhysicalDevice device, u32& outFamily) const {
        u32 count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
        for (u32 i = 0; i < count; ++i) {
            if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) continue;
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &present);
            if (present == VK_TRUE) {
                outFamily = i;
                return true;
            }
        }
        return false;
    }

    static bool supportsSwapchain(VkPhysicalDevice device) {
        u32 count = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> extensions(count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
        for (const VkExtensionProperties& ext : extensions) {
            if (std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) return true;
        }
        return false;
    }

    bool createLogicalDevice() {
        const f32 priority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamily_;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;

        const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        info.queueCreateInfoCount = 1;
        info.pQueueCreateInfos = &queueInfo;
        info.enabledExtensionCount = 1;
        info.ppEnabledExtensionNames = deviceExtensions;
        ZK_VK(vkCreateDevice(physicalDevice_, &info, nullptr, &device_));
        vkGetDeviceQueue(device_, queueFamily_, 0, &graphicsQueue_);
        return true;
    }

    [[nodiscard]] u32 findMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties memory{};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memory);
        for (u32 i = 0; i < memory.memoryTypeCount; ++i) {
            const bool typeOk = (typeFilter & (1u << i)) != 0;
            const bool propsOk = (memory.memoryTypes[i].propertyFlags & properties) == properties;
            if (typeOk && propsOk) return i;
        }
        return 0;
    }

    bool allocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                        VkBuffer& outBuffer, VkDeviceMemory& outMemory) {
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = size;
        info.usage = usage;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &info, nullptr, &outBuffer) != VK_SUCCESS) return false;

        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device_, outBuffer, &requirements);
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = requirements.size;
        alloc.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, properties);
        if (vkAllocateMemory(device_, &alloc, nullptr, &outMemory) != VK_SUCCESS) {
            vkDestroyBuffer(device_, outBuffer, nullptr);
            outBuffer = VK_NULL_HANDLE;
            return false;
        }
        vkBindBufferMemory(device_, outBuffer, outMemory, 0);
        return true;
    }

    // Create an image + backing device-local memory + a view in one shot.
    bool allocateImage(u32 width, u32 height, VkFormat format, VkImageUsageFlags usage,
                       VkImageAspectFlags aspect, VkImage& outImage, VkDeviceMemory& outMemory,
                       VkImageView& outView) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {width, height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(device_, &imageInfo, nullptr, &outImage) != VK_SUCCESS) return false;

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, outImage, &requirements);
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = requirements.size;
        alloc.memoryTypeIndex =
            findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(device_, &alloc, nullptr, &outMemory) != VK_SUCCESS) {
            vkDestroyImage(device_, outImage, nullptr);
            outImage = VK_NULL_HANDLE;
            return false;
        }
        vkBindImageMemory(device_, outImage, outMemory, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = outImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &viewInfo, nullptr, &outView) != VK_SUCCESS) {
            vkDestroyImage(device_, outImage, nullptr);
            vkFreeMemory(device_, outMemory, nullptr);
            outImage = VK_NULL_HANDLE;
            outMemory = VK_NULL_HANDLE;
            return false;
        }
        return true;
    }

    void destroyTextureResources(const VulkanTexture& texture) {
        vkDestroySampler(device_, texture.sampler, nullptr);
        vkDestroyImageView(device_, texture.view, nullptr);
        vkDestroyImage(device_, texture.image, nullptr);
        vkFreeMemory(device_, texture.memory, nullptr);
    }

    // Free a render target's own resources (framebuffer / render pass / depth) and
    // its color texture entry. Safe to call on a partially-built target.
    void destroyRenderTargetResources(VulkanRenderTarget& target) {
        if (target.framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, target.framebuffer, nullptr);
        }
        if (target.renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device_, target.renderPass, nullptr);
        }
        if (target.depthView != VK_NULL_HANDLE) vkDestroyImageView(device_, target.depthView, nullptr);
        if (target.depthImage != VK_NULL_HANDLE) vkDestroyImage(device_, target.depthImage, nullptr);
        if (target.depthMemory != VK_NULL_HANDLE) vkFreeMemory(device_, target.depthMemory, nullptr);
        const auto it = textures_.find(target.colorTextureId);
        if (it != textures_.end()) {
            destroyTextureResources(it->second);
            textures_.erase(it);
        }
    }

    // Render pass for an offscreen target: color (→ SHADER_READ_ONLY, ready to
    // sample) + depth, with dependencies syncing sampling around the color writes.
    bool buildRenderTargetPass(VkRenderPass& out) {
        VkAttachmentDescription color{};
        color.format = surfaceFormat_.format;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentDescription depth{};
        depth.format = depthFormat_;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference depthRef{};
        depthRef.attachment = 1;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        // Incoming: wait for any prior sampling of this image before we overwrite it.
        // Outgoing: make a later pass's sampling wait for our color writes.
        VkSubpassDependency deps[2]{};
        deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        deps[0].dstSubpass = 0;
        deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        deps[1].srcSubpass = 0;
        deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        const VkAttachmentDescription attachments[] = {color, depth};
        VkRenderPassCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 2;
        info.pAttachments = attachments;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 2;
        info.pDependencies = deps;
        ZK_VK(vkCreateRenderPass(device_, &info, nullptr, &out));
        return true;
    }

    void setViewportScissor(VkCommandBuffer cmd, VkExtent2D extent) {
        VkViewport viewport{};
        viewport.width = static_cast<f32>(extent.width);
        viewport.height = static_cast<f32>(extent.height);
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        VkRect2D scissor{};
        scissor.extent = extent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);
    }

    // Transient command buffer for one-off transfers (blocks until done).
    [[nodiscard]] VkCommandBuffer beginSingleTime() {
        VkCommandBufferAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.commandPool = commandPool_;
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(device_, &alloc, &cmd);
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin);
        return cmd;
    }

    void endSingleTime(VkCommandBuffer cmd) {
        vkEndCommandBuffer(cmd);
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        vkQueueSubmit(graphicsQueue_, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue_);
        vkFreeCommandBuffers(device_, commandPool_, 1, &cmd);
    }

    void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkCommandBuffer cmd = beginSingleTime();
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                   newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        endSingleTime(cmd);
    }

    void copyBufferToImage(VkBuffer buffer, VkImage image, u32 width, u32 height) {
        VkCommandBuffer cmd = beginSingleTime();
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {width, height, 1};
        vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        endSingleTime(cmd);
    }

    bool createDescriptorPool() {
        VkDescriptorPoolSize sizes[2]{};
        sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        sizes[0].descriptorCount = 64;
        sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sizes[1].descriptorCount = 64;
        VkDescriptorPoolCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        info.maxSets = 64;
        info.poolSizeCount = 2;
        info.pPoolSizes = sizes;
        ZK_VK(vkCreateDescriptorPool(device_, &info, nullptr, &descriptorPool_));
        return true;
    }

    // --- Swapchain -------------------------------------------------------
    bool createSwapchain() {
        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &caps);

        u32 formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data());
        VkSurfaceFormatKHR chosen = formats.empty() ? VkSurfaceFormatKHR{} : formats[0];
        for (const VkSurfaceFormatKHR& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosen = f;
                break;
            }
        }
        surfaceFormat_ = chosen;

        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        if (!config_.vsync) {
            u32 modeCount = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &modeCount, nullptr);
            std::vector<VkPresentModeKHR> modes(modeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &modeCount,
                                                      modes.data());
            for (VkPresentModeKHR m : modes) {
                if (m == VK_PRESENT_MODE_MAILBOX_KHR) presentMode = m;
            }
        }

        VkExtent2D extent{width_, height_};
        if (caps.currentExtent.width != UINT32_MAX) {
            extent = caps.currentExtent;
        } else {
            extent.width =
                std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
            extent.height =
                std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
        }
        extent_ = extent;

        u32 imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
            imageCount = caps.maxImageCount;
        }

        VkSwapchainCreateInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = surface_;
        info.minImageCount = imageCount;
        info.imageFormat = surfaceFormat_.format;
        info.imageColorSpace = surfaceFormat_.colorSpace;
        info.imageExtent = extent_;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = caps.currentTransform;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = presentMode;
        info.clipped = VK_TRUE;
        info.oldSwapchain = VK_NULL_HANDLE;
        ZK_VK(vkCreateSwapchainKHR(device_, &info, nullptr, &swapchain_));

        u32 count = 0;
        vkGetSwapchainImagesKHR(device_, swapchain_, &count, nullptr);
        images_.resize(count);
        vkGetSwapchainImagesKHR(device_, swapchain_, &count, images_.data());

        imageViews_.resize(count);
        for (u32 i = 0; i < count; ++i) {
            VkImageViewCreateInfo view{};
            view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view.image = images_[i];
            view.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view.format = surfaceFormat_.format;
            view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view.subresourceRange.levelCount = 1;
            view.subresourceRange.layerCount = 1;
            ZK_VK(vkCreateImageView(device_, &view, nullptr, &imageViews_[i]));
        }

        imagesInFlight_.assign(count, VK_NULL_HANDLE);
        return true;
    }

    // Pick a supported depth format, preferring a pure 32-bit float depth.
    [[nodiscard]] VkFormat findDepthFormat() const {
        const VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
                                       VK_FORMAT_D24_UNORM_S8_UINT};
        for (VkFormat format : candidates) {
            VkFormatProperties props{};
            vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &props);
            if ((props.optimalTilingFeatures &
                 VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
                return format;
            }
        }
        return VK_FORMAT_D32_SFLOAT;
    }

    // A depth attachment sized to the swapchain; recreated alongside it. The
    // render pass transitions it from UNDEFINED, so no manual barrier is needed.
    bool createDepthResources() {
        depthFormat_ = findDepthFormat();

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {extent_.width, extent_.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = depthFormat_;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ZK_VK(vkCreateImage(device_, &imageInfo, nullptr, &depthImage_));

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, depthImage_, &requirements);
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = requirements.size;
        alloc.memoryTypeIndex =
            findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        ZK_VK(vkAllocateMemory(device_, &alloc, nullptr, &depthMemory_));
        vkBindImageMemory(device_, depthImage_, depthMemory_, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImage_;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = depthFormat_;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        ZK_VK(vkCreateImageView(device_, &viewInfo, nullptr, &depthView_));
        return true;
    }

    bool createRenderPass() {
        VkAttachmentDescription color{};
        color.format = surfaceFormat_.format;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depth{};
        depth.format = depthFormat_;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // not sampled after the pass
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthRef{};
        depthRef.attachment = 1;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        // Wait on both color output and depth tests before the subpass writes them.
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                   VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        const VkAttachmentDescription attachments[] = {color, depth};
        VkRenderPassCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 2;
        info.pAttachments = attachments;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;
        ZK_VK(vkCreateRenderPass(device_, &info, nullptr, &renderPass_));
        return true;
    }

    [[nodiscard]] VkShaderModule createShaderModule(std::span<const u32> words) {
        VkShaderModuleCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = words.size() * sizeof(u32);
        info.pCode = words.data();
        VkShaderModule module = VK_NULL_HANDLE;
        if (vkCreateShaderModule(device_, &info, nullptr, &module) != VK_SUCCESS) {
            return VK_NULL_HANDLE;
        }
        return module;
    }

    // Build a graphics pipeline (dynamic viewport/scissor so it survives resize).
    bool buildPipeline(const PipelineDesc& desc, VulkanPipeline& out) {
        VkShaderModule vert = createShaderModule(desc.vertexSpirv);
        VkShaderModule frag = createShaderModule(desc.fragmentSpirv);
        if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE) {
            if (vert != VK_NULL_HANDLE) vkDestroyShaderModule(device_, vert, nullptr);
            if (frag != VK_NULL_HANDLE) vkDestroyShaderModule(device_, frag, nullptr);
            error_ = "render: invalid shader SPIR-V";
            return false;
        }

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag;
        stages[1].pName = "main";

        // Vertex input from the layout (no binding at all if there are no attributes).
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = desc.vertexLayout.stride;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributes;
        attributes.reserve(desc.vertexLayout.attributes.size());
        for (const VertexAttribute& attr : desc.vertexLayout.attributes) {
            VkVertexInputAttributeDescription vk{};
            vk.location = attr.location;
            vk.binding = 0;
            vk.format = toVkFormat(attr.format);
            vk.offset = attr.offset;
            attributes.push_back(vk);
        }
        const bool hasVertices = !attributes.empty();

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = hasVertices ? 1u : 0u;
        vertexInput.pVertexBindingDescriptions = hasVertices ? &binding : nullptr;
        vertexInput.vertexAttributeDescriptionCount = static_cast<u32>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = hasVertices ? attributes.data() : nullptr;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = toVkTopology(desc.topology);

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Depth test/write against the swapchain's depth attachment. LESS_OR_EQUAL
        // so coplanar 2D geometry at a fixed depth still draws over the clear.
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = desc.depthTest ? VK_TRUE : VK_FALSE;
        depthStencil.depthWriteEnable = desc.depthWrite ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo colorBlend{};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.attachmentCount = 1;
        colorBlend.pAttachments = &blendAttachment;

        const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates = dynamicStates;

        // Descriptor set layout (set 0) from the declared resource bindings.
        if (!desc.bindings.empty()) {
            std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
            layoutBindings.reserve(desc.bindings.size());
            for (usize i = 0; i < desc.bindings.size(); ++i) {
                VkDescriptorSetLayoutBinding b{};
                b.binding = static_cast<u32>(i);
                b.descriptorType = toVkDescriptorType(desc.bindings[i]);
                b.descriptorCount = 1;
                b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
                layoutBindings.push_back(b);
            }
            VkDescriptorSetLayoutCreateInfo setInfo{};
            setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            setInfo.bindingCount = static_cast<u32>(layoutBindings.size());
            setInfo.pBindings = layoutBindings.data();
            if (vkCreateDescriptorSetLayout(device_, &setInfo, nullptr, &out.setLayout) !=
                VK_SUCCESS) {
                vkDestroyShaderModule(device_, vert, nullptr);
                vkDestroyShaderModule(device_, frag, nullptr);
                error_ = "render: vkCreateDescriptorSetLayout failed";
                return false;
            }
        }

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = out.setLayout != VK_NULL_HANDLE ? 1u : 0u;
        layoutInfo.pSetLayouts = out.setLayout != VK_NULL_HANDLE ? &out.setLayout : nullptr;
        if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &out.layout) != VK_SUCCESS) {
            if (out.setLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device_, out.setLayout, nullptr);
            }
            vkDestroyShaderModule(device_, vert, nullptr);
            vkDestroyShaderModule(device_, frag, nullptr);
            error_ = "render: vkCreatePipelineLayout failed";
            return false;
        }

        VkGraphicsPipelineCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        info.stageCount = 2;
        info.pStages = stages;
        info.pVertexInputState = &vertexInput;
        info.pInputAssemblyState = &inputAssembly;
        info.pViewportState = &viewportState;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &multisample;
        info.pDepthStencilState = &depthStencil;
        info.pColorBlendState = &colorBlend;
        info.pDynamicState = &dynamicState;
        info.layout = out.layout;
        info.renderPass = renderPass_;
        info.subpass = 0;
        const VkResult result =
            vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &out.pipeline);

        vkDestroyShaderModule(device_, vert, nullptr);
        vkDestroyShaderModule(device_, frag, nullptr);
        if (result != VK_SUCCESS) {
            vkDestroyPipelineLayout(device_, out.layout, nullptr);
            if (out.setLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device_, out.setLayout, nullptr);
            }
            out.layout = VK_NULL_HANDLE;
            out.setLayout = VK_NULL_HANDLE;
            error_ = "render: vkCreateGraphicsPipelines failed";
            return false;
        }
        return true;
    }

    bool createFramebuffers() {
        framebuffers_.resize(imageViews_.size());
        for (usize i = 0; i < imageViews_.size(); ++i) {
            const VkImageView attachments[] = {imageViews_[i], depthView_};
            VkFramebufferCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.renderPass = renderPass_;
            info.attachmentCount = 2;
            info.pAttachments = attachments;
            info.width = extent_.width;
            info.height = extent_.height;
            info.layers = 1;
            ZK_VK(vkCreateFramebuffer(device_, &info, nullptr, &framebuffers_[i]));
        }
        return true;
    }

    bool createCommandResources() {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamily_;
        ZK_VK(vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_));

        commandBuffers_.resize(kFramesInFlight);
        VkCommandBufferAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.commandPool = commandPool_;
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = kFramesInFlight;
        ZK_VK(vkAllocateCommandBuffers(device_, &alloc, commandBuffers_.data()));
        return true;
    }

    bool createSyncObjects() {
        imageAvailable_.resize(kFramesInFlight);
        renderFinished_.resize(kFramesInFlight);
        inFlight_.resize(kFramesInFlight);

        VkSemaphoreCreateInfo sem{};
        sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fence{};
        fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (u32 i = 0; i < kFramesInFlight; ++i) {
            ZK_VK(vkCreateSemaphore(device_, &sem, nullptr, &imageAvailable_[i]));
            ZK_VK(vkCreateSemaphore(device_, &sem, nullptr, &renderFinished_[i]));
            ZK_VK(vkCreateFence(device_, &fence, nullptr, &inFlight_[i]));
        }
        return true;
    }

    void recreateSwapchain() {
        if (device_ == VK_NULL_HANDLE) return;
        vkDeviceWaitIdle(device_);
        destroySwapchain();
        if (width_ == 0 || height_ == 0) return;  // minimized
        createSwapchain();
        createDepthResources();
        createFramebuffers();
    }

    void destroySwapchain() {
        if (depthView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, depthView_, nullptr);
            depthView_ = VK_NULL_HANDLE;
        }
        if (depthImage_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_, depthImage_, nullptr);
            depthImage_ = VK_NULL_HANDLE;
        }
        if (depthMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device_, depthMemory_, nullptr);
            depthMemory_ = VK_NULL_HANDLE;
        }
        for (VkFramebuffer fb : framebuffers_) {
            if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(device_, fb, nullptr);
        }
        framebuffers_.clear();
        for (VkImageView view : imageViews_) {
            if (view != VK_NULL_HANDLE) vkDestroyImageView(device_, view, nullptr);
        }
        imageViews_.clear();
        if (swapchain_ != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device_, swapchain_, nullptr);
            swapchain_ = VK_NULL_HANDLE;
        }
    }

    void teardown() {
        if (device_ != VK_NULL_HANDLE) vkDeviceWaitIdle(device_);

        // Bind groups' sets are freed with the pool below; just drop the map.
        bindGroups_.clear();
        if (descriptorPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        }
        // Render targets own framebuffer/render pass/depth and erase their color
        // texture from textures_ (freed here), so the loop below handles the rest.
        for (auto& [id, target] : renderTargets_) {
            destroyRenderTargetResources(target);
        }
        renderTargets_.clear();
        for (auto& [id, texture] : textures_) {
            vkDestroySampler(device_, texture.sampler, nullptr);
            vkDestroyImageView(device_, texture.view, nullptr);
            vkDestroyImage(device_, texture.image, nullptr);
            vkFreeMemory(device_, texture.memory, nullptr);
        }
        textures_.clear();
        for (auto& [id, buffer] : buffers_) {
            vkDestroyBuffer(device_, buffer.buffer, nullptr);
            vkFreeMemory(device_, buffer.memory, nullptr);
        }
        buffers_.clear();
        for (auto& [id, pipeline] : pipelines_) {
            vkDestroyPipeline(device_, pipeline.pipeline, nullptr);
            vkDestroyPipelineLayout(device_, pipeline.layout, nullptr);
            if (pipeline.setLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device_, pipeline.setLayout, nullptr);
            }
        }
        pipelines_.clear();

        for (VkSemaphore s : imageAvailable_) {
            if (s != VK_NULL_HANDLE) vkDestroySemaphore(device_, s, nullptr);
        }
        for (VkSemaphore s : renderFinished_) {
            if (s != VK_NULL_HANDLE) vkDestroySemaphore(device_, s, nullptr);
        }
        for (VkFence f : inFlight_) {
            if (f != VK_NULL_HANDLE) vkDestroyFence(device_, f, nullptr);
        }
        if (commandPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, commandPool_, nullptr);
        destroySwapchain();
        if (renderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, renderPass_, nullptr);
        if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
        if (surface_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance_, surface_, nullptr);
#if ZUKIRU_RENDER_VK_XLIB
        if (surfaceDisplayX11_ != nullptr) {
            XCloseDisplay(static_cast<Display*>(surfaceDisplayX11_));
            surfaceDisplayX11_ = nullptr;
        }
#endif
        if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
    }

    // Config / context.
    DeviceConfig config_{};
    const platform::Window* window_ = nullptr;
    std::string error_;
    std::string deviceName_;
    u32 width_ = 0;
    u32 height_ = 0;
    void* surfaceDisplayX11_ = nullptr;

    // Vulkan objects.
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    u32 queueFamily_ = 0;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkSurfaceFormatKHR surfaceFormat_{};
    VkExtent2D extent_{};
    std::vector<VkImage> images_;
    std::vector<VkImageView> imageViews_;

    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
    VkImageView depthView_ = VK_NULL_HANDLE;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;

    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;

    std::vector<VkSemaphore> imageAvailable_;
    std::vector<VkSemaphore> renderFinished_;
    std::vector<VkFence> inFlight_;
    std::vector<VkFence> imagesInFlight_;

    // Resources.
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::unordered_map<u32, VulkanBuffer> buffers_;
    std::unordered_map<u32, VulkanPipeline> pipelines_;
    std::unordered_map<u32, VulkanTexture> textures_;
    std::unordered_map<u32, VulkanBindGroup> bindGroups_;
    std::unordered_map<u32, VulkanRenderTarget> renderTargets_;
    u32 nextBufferId_ = 1;
    u32 nextPipelineId_ = 1;
    u32 nextTextureId_ = 1;
    u32 nextBindGroupId_ = 1;
    u32 nextRenderTargetId_ = 1;

    Color clearColor_{0.0f, 0.0f, 0.0f, 1.0f};
    u32 currentFrame_ = 0;
    u32 imageIndex_ = 0;
};

#undef ZK_VK

}  // namespace

Result<std::unique_ptr<Device>> createVulkanDevice(const platform::Window& window,
                                                   const DeviceConfig& config) {
    auto device = std::make_unique<VulkanDevice>();
    if (!device->init(window, config)) {
        return Err(Error{"Vulkan device init failed: " + device->error()});
    }
    return Ok(std::unique_ptr<Device>{std::move(device)});
}

}  // namespace zukiru::render
