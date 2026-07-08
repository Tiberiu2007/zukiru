// Vulkan RHI backend factory. Private to the render module.
#pragma once

#include <zuki/render/rhi.hpp>

#include <memory>

namespace zuki::platform {
class Window;
}

namespace zuki::render {

[[nodiscard]] Result<std::unique_ptr<Device>> createVulkanDevice(const platform::Window& window,
                                                                 const DeviceConfig& config);

}  // namespace zuki::render
