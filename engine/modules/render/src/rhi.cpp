#include <zukiru/render/rhi.hpp>

#include "vulkan/vulkan_device.hpp"

namespace zukiru::render {

Result<std::unique_ptr<Device>> createDevice(const platform::Window& window,
                                             const DeviceConfig& config) {
    switch (config.backend) {
        case Backend::Vulkan:
            return createVulkanDevice(window, config);
    }
    return Err(Error{"render: unsupported backend"});
}

}  // namespace zukiru::render
