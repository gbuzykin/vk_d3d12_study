#pragma once

#include "rendering_iface.h"
#include "vulkan_api.h"

#include <vector>

namespace app3d::rel::vulkan {

class PhysicalDevice;

class Surface : public ISurface {
 public:
    Surface(PhysicalDevice& physical_device, VkSurfaceKHR surface);

    const std::vector<std::uint32_t>& getPresentQueueFamilies() const { return present_queue_families_; }

    bool obtainCapabilities();
    bool obtainPresentQueueFamilies();
    bool obtainPresentModes();

    //@{ IRenderingSurface
    //@}

 private:
    PhysicalDevice& physical_device_;
    VkSurfaceKHR surface_;
    VkSurfaceCapabilitiesKHR capabilities_;
    std::vector<std::uint32_t> present_queue_families_;
    std::vector<VkPresentModeKHR> present_modes_;
};

}  // namespace app3d::rel::vulkan
