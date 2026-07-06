// Vulkan backend for the RHI: brings up instance → surface → device → swapchain →
// render pass and clears/presents each frame. See docs/adr/0006-render-architecture.md.
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

#include "triangle_shaders.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace zukiru::render {
namespace {

constexpr u32 kFramesInFlight = 2;

// Error-checked Vulkan call: records a message and bails out of init() on failure.
#define ZK_VK(expr)                                                              \
    do {                                                                         \
        const VkResult zkRes = (expr);                                           \
        if (zkRes != VK_SUCCESS) {                                               \
            error_ = std::string(#expr) + " -> VkResult " + std::to_string(zkRes); \
            return false;                                                        \
        }                                                                        \
    } while (false)

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
        if (!createRenderPass()) return false;
        if (!createPipeline()) return false;
        if (!createFramebuffers()) return false;
        if (!createCommandResources()) return false;
        if (!createSyncObjects()) return false;
        return true;
    }

    // --- Device interface ------------------------------------------------

    bool beginFrame() override {
        vkWaitForFences(device_, 1, &inFlight_[currentFrame_], VK_TRUE, UINT64_MAX);

        const VkResult acquire =
            vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                  imageAvailable_[currentFrame_], VK_NULL_HANDLE, &imageIndex_);
        if (acquire == VK_ERROR_OUT_OF_DATE_KHR) return false;  // caller should resize()
        if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) return false;

        // Don't render into an image still being presented from an earlier frame.
        if (imagesInFlight_[imageIndex_] != VK_NULL_HANDLE) {
            vkWaitForFences(device_, 1, &imagesInFlight_[imageIndex_], VK_TRUE, UINT64_MAX);
        }
        imagesInFlight_[imageIndex_] = inFlight_[currentFrame_];

        recordClear(commandBuffers_[currentFrame_], imageIndex_);
        return true;
    }

    void endFrame() override {
        vkResetFences(device_, 1, &inFlight_[currentFrame_]);

        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &imageAvailable_[currentFrame_];
        submit.pWaitDstStageMask = &waitStage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commandBuffers_[currentFrame_];
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
            recreateSwapchain();  // resized/stale — refresh for the next frame
        }

        currentFrame_ = (currentFrame_ + 1) % kFramesInFlight;
    }

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
    // Avoid including platform/window.hpp types by value in members; mirror the
    // small extent struct we read.
    struct WindowExtentLike {
        u32 width;
        u32 height;
    };

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
                // Use our OWN Display connection for the surface, not the window's.
                // The NVIDIA driver installs an XCloseDisplay hook when an Xlib
                // surface is created; by owning this connection we can close it
                // while the instance (and thus the driver) is still loaded, and
                // the window's own display never gets a dangling hook.
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
                return true;  // prefer a discrete GPU
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

    // --- Swapchain -------------------------------------------------------
    bool createSwapchain() {
        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &caps);

        // Format: prefer BGRA8 sRGB.
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

        // Present mode: FIFO for vsync (always available); else prefer MAILBOX.
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

        // Extent.
        VkExtent2D extent{width_, height_};
        if (caps.currentExtent.width != UINT32_MAX) {
            extent = caps.currentExtent;
        } else {
            extent.width = std::clamp(extent.width, caps.minImageExtent.width,
                                      caps.maxImageExtent.width);
            extent.height = std::clamp(extent.height, caps.minImageExtent.height,
                                       caps.maxImageExtent.height);
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

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &color;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;
        ZK_VK(vkCreateRenderPass(device_, &info, nullptr, &renderPass_));
        return true;
    }

    [[nodiscard]] VkShaderModule createShaderModule(const u32* words, usize count) {
        VkShaderModuleCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = count * sizeof(u32);
        info.pCode = words;
        VkShaderModule module = VK_NULL_HANDLE;
        if (vkCreateShaderModule(device_, &info, nullptr, &module) != VK_SUCCESS) {
            return VK_NULL_HANDLE;
        }
        return module;
    }

    // The built-in RGB-triangle pipeline. Viewport/scissor are dynamic, so it
    // survives swapchain recreation without being rebuilt.
    bool createPipeline() {
        VkShaderModule vert =
            createShaderModule(kTriangleVertSpirv, sizeof(kTriangleVertSpirv) / sizeof(u32));
        VkShaderModule frag =
            createShaderModule(kTriangleFragSpirv, sizeof(kTriangleFragSpirv) / sizeof(u32));
        if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE) {
            error_ = "render: failed to create shader modules";
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

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;  // set dynamically each frame
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

        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo colorBlend{};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.attachmentCount = 1;
        colorBlend.pAttachments = &blendAttachment;

        const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                                VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates = dynamicStates;

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        ZK_VK(vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout_));

        VkGraphicsPipelineCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        info.stageCount = 2;
        info.pStages = stages;
        info.pVertexInputState = &vertexInput;
        info.pInputAssemblyState = &inputAssembly;
        info.pViewportState = &viewportState;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &multisample;
        info.pColorBlendState = &colorBlend;
        info.pDynamicState = &dynamicState;
        info.layout = pipelineLayout_;
        info.renderPass = renderPass_;
        info.subpass = 0;
        const VkResult result =
            vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline_);

        vkDestroyShaderModule(device_, vert, nullptr);
        vkDestroyShaderModule(device_, frag, nullptr);
        if (result != VK_SUCCESS) {
            error_ = "render: vkCreateGraphicsPipelines failed";
            return false;
        }
        return true;
    }

    bool createFramebuffers() {
        framebuffers_.resize(imageViews_.size());
        for (usize i = 0; i < imageViews_.size(); ++i) {
            VkFramebufferCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.renderPass = renderPass_;
            info.attachmentCount = 1;
            info.pAttachments = &imageViews_[i];
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
        fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // so the first wait returns immediately

        for (u32 i = 0; i < kFramesInFlight; ++i) {
            ZK_VK(vkCreateSemaphore(device_, &sem, nullptr, &imageAvailable_[i]));
            ZK_VK(vkCreateSemaphore(device_, &sem, nullptr, &renderFinished_[i]));
            ZK_VK(vkCreateFence(device_, &fence, nullptr, &inFlight_[i]));
        }
        return true;
    }

    void recordClear(VkCommandBuffer cmd, u32 imageIndex) {
        vkResetCommandBuffer(cmd, 0);
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cmd, &begin);

        VkClearValue clear{};
        clear.color = {{clearColor_.r, clearColor_.g, clearColor_.b, clearColor_.a}};

        VkRenderPassBeginInfo pass{};
        pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        pass.renderPass = renderPass_;
        pass.framebuffer = framebuffers_[imageIndex];
        pass.renderArea.extent = extent_;
        pass.clearValueCount = 1;
        pass.pClearValues = &clear;
        vkCmdBeginRenderPass(cmd, &pass, VK_SUBPASS_CONTENTS_INLINE);

        // Draw the built-in triangle. Viewport/scissor are dynamic, tracking the
        // current swapchain extent.
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        VkViewport viewport{};
        viewport.width = static_cast<f32>(extent_.width);
        viewport.height = static_cast<f32>(extent_.height);
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        VkRect2D scissor{};
        scissor.extent = extent_;
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);
    }

    void recreateSwapchain() {
        if (device_ == VK_NULL_HANDLE) return;
        vkDeviceWaitIdle(device_);
        destroySwapchain();
        // A minimized window reports a zero extent; skip until it has area again.
        if (width_ == 0 || height_ == 0) return;
        createSwapchain();
        createFramebuffers();
    }

    void destroySwapchain() {
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
        if (pipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, pipeline_, nullptr);
        if (pipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        }
        if (renderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, renderPass_, nullptr);
        if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
        if (surface_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance_, surface_, nullptr);
        // Close our surface Display *before* destroying the instance, so the
        // driver's XCloseDisplay hook still points at loaded ICD code.
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

    // Our own X11 Display connection for the surface (X11 backend only); a
    // Display* stored type-erased so this member exists regardless of build config.
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

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;

    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;

    std::vector<VkSemaphore> imageAvailable_;
    std::vector<VkSemaphore> renderFinished_;
    std::vector<VkFence> inFlight_;
    std::vector<VkFence> imagesInFlight_;

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
