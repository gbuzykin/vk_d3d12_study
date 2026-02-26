#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

#include <vector>

namespace app3d::rel::vulkan {

class RenderingDriver;
class PhysicalDevice;
class Device;
class SwapChain;

class Surface : public ISurface {
 public:
    Surface(RenderingDriver& instance, VkSurfaceKHR surface);
    ~Surface() override;

    const VkSurfaceCapabilitiesKHR& getCapabilities() const { return capabilities_; }
    const std::vector<VkPresentModeKHR>& getPresentModes() const { return present_modes_; }
    const std::vector<VkSurfaceFormatKHR>& getFormats() const { return formats_; }
    std::uint32_t getPresentQueueFamily(std::uint32_t n = 0) const;

    bool obtainCapabilities(PhysicalDevice& physical_device);
    bool obtainPresentQueueFamilies(PhysicalDevice& physical_device);
    bool obtainPresentModes(PhysicalDevice& physical_device);
    bool obtainFormats(PhysicalDevice& physical_device);
    SwapChain* createSwapChain(Device& device, const SwapChainCreateInfo& create_info);
    void destroySwapChain();

    VkSurfaceKHR getHandle() { return surface_; }

    //@{ ISurface
    //@}

 private:
    RenderingDriver& instance_;
    VkSurfaceKHR surface_;
    VkSurfaceCapabilitiesKHR capabilities_{};
    std::vector<std::uint32_t> present_queue_families_;
    std::vector<VkPresentModeKHR> present_modes_;
    std::vector<VkSurfaceFormatKHR> formats_;
    std::unique_ptr<SwapChain> swap_chain_;

    std::uint32_t chooseImageCount(const SwapChainCreateInfo& create_info);
    VkExtent2D chooseImageSize(const SwapChainCreateInfo& create_info);
    VkSurfaceFormatKHR chooseImageFormat(const SwapChainCreateInfo& create_info);
    VkPresentModeKHR choosePresentMode(const SwapChainCreateInfo& create_info);
};

}  // namespace app3d::rel::vulkan
