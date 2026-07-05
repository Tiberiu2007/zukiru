// Vulkan RHI backend factory. Private to the render module.
#pragma once

#include <zukiru/render/rhi.hpp>

#include <memory>

namespace zukiru::platform {
class Window;
}

namespace zukiru::render {

[[nodiscard]] Result<std::unique_ptr<Device>> createVulkanDevice(const platform::Window& window,
                                                                 const DeviceConfig& config);

}  // namespace zukiru::render
