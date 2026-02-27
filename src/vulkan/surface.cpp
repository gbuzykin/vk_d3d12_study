#include "surface.h"

#include "rendering_driver.h"

#include "utils/print.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Surface class implementation

Surface::Surface(PhysicalDevice& physical_device, VkSurfaceKHR surface)
    : physical_device_(physical_device), surface_(surface) {}

bool Surface::obtainCapabilities() {
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_.getHandle(), surface_, &capabilities_);
    if (result != VK_SUCCESS) {
        printError("couldn't get the capabilities of a surface");
        return false;
    }

    return true;
}

bool Surface::obtainPresentQueueFamilies() {
    const auto& queue_families = physical_device_.getQueueFamilies();

    for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(queue_families.size()); ++index) {
        VkBool32 is_supported = VK_FALSE;
        VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device_.getHandle(), index, surface_,
                                                               &is_supported);
        if (result == VK_SUCCESS && is_supported == VK_TRUE) { present_queue_families_.push_back(index); }
    }

    if (present_queue_families_.empty()) {
        printError("couldn't obtain present queue families");
        return false;
    }

    return true;
}

bool Surface::obtainPresentModes() {
    std::uint32_t present_mode_count = 0;
    VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_.getHandle(), surface_,
                                                                &present_mode_count, nullptr);
    if (result != VK_SUCCESS || present_mode_count == 0) {
        printError("couldn't get the number of supported present modes");
        return false;
    }

    present_modes_.resize(present_mode_count);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_.getHandle(), surface_, &present_mode_count,
                                                       present_modes_.data());
    if (result != VK_SUCCESS || present_mode_count == 0) {
        printError("couldn't enumerate present modes");
        return false;
    }

    return true;
}
