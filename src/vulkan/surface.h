#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

#include <vector>

namespace app3d::rel::vulkan {

class RenderingDriver;
class PhysicalDevice;

class Surface : public ISurface {
 public:
    explicit Surface(RenderingDriver& instance);
    ~Surface() override;

    std::uint32_t getPresentQueueFamily(std::uint32_t n = 0) const;
    const VkSurfaceCapabilitiesKHR& getCapabilities() const { return capabilities_; }
    VkImageUsageFlags getImageUsage() const { return image_usage_; }
    VkSurfaceFormatKHR getImageFormat() const { return selected_format_; }
    VkPresentModeKHR getPresentMode() const { return selected_present_mode_; }

    bool create(const WindowDescriptor& win_desc);
    bool loadCapabilities(PhysicalDevice& physical_device);
    bool loadFormats(PhysicalDevice& physical_device);
    bool loadPresentQueueFamilies(PhysicalDevice& physical_device);
    bool loadPresentModes(PhysicalDevice& physical_device);
    bool checkAndSelectSurfaceFeatures();

    VkSurfaceKHR operator~() { return surface_; }

    //@{ ISurface
    //@}

 private:
    RenderingDriver& instance_;
    VkSurfaceKHR surface_{VK_NULL_HANDLE};
    VkSurfaceCapabilitiesKHR capabilities_{};
    std::vector<VkSurfaceFormatKHR> formats_;
    std::vector<std::uint32_t> present_queue_families_;
    std::vector<VkPresentModeKHR> present_modes_;
    VkImageUsageFlags image_usage_{};
    VkSurfaceFormatKHR selected_format_{};
    VkPresentModeKHR selected_present_mode_{};
};

}  // namespace app3d::rel::vulkan
