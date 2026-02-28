#include "device.h"

#include "rendering_driver.h"
#include "surface.h"

#include "utils/print.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Device class implementation

Device::Device(PhysicalDevice& physical_device) : physical_device_(physical_device) {}

Device::~Device() {
    if (device_) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
}

bool Device::create(const DesiredDeviceCaps& caps) {
    DeviceCreateInfo create_info;

    create_info.extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    create_info.features.geometryShader = VK_TRUE;

    create_info.queue_infos.reserve(8);

    std::uint32_t graphics_queue_family_index = physical_device_.findSuitableQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    if (graphics_queue_family_index == INVALID_UINT32_VALUE) {
        logError("couldn't obtain graphics queue family index");
        return false;
    }

    create_info.queue_infos.push_back({graphics_queue_family_index, {1.0f}});

    const auto& present_queue_families = surface.getPresentQueueFamilies();
    if (present_queue_families.empty()) {
        printError("couldn't obtain presentation queue family index");
        return false;
    }

    if (present_queue_families.front() != graphics_queue_family_index) {
        create_info.queue_infos.push_back({present_queue_families.front(), {1.0f}});
    }

    std::uint32_t compute_queue_family_index = 0;

    if (caps.needs_compute) {
        compute_queue_family_index = physical_device_.findSuitableQueueFamily(VK_QUEUE_COMPUTE_BIT);
        if (compute_queue_family_index == INVALID_UINT32_VALUE) {
            logError("couldn't obtain compute queue family index");
            return false;
        }

        if (compute_queue_family_index != graphics_queue_family_index &&
            compute_queue_family_index != present_queue_families.front()) {
            create_info.queue_infos.push_back({compute_queue_family_index, {1.0f}});
        }
    }

    if (!physical_device_.createLogicalDevice(create_info, device_)) { return false; }

    vkGetDeviceQueue(device_, graphics_queue_family_index, 0, &graphics_queue_);

    vkGetDeviceQueue(device_, present_queue_families.front(), 0, &present_queue_);

    if (caps.needs_compute) { vkGetDeviceQueue(device_, compute_queue_family_index, 0, &compute_queue_); }

    return true;
}

ISwapChain* Device::createSwapChain(const SwapChainCreateInfo& create_info) {
// Get caps
if( 0xFFFFFFFF == surface_capabilities.currentExtent.width ) {
size_of_images = { 640, 480 };
if( size_of_images.width < surface_capabilities.minImageExtent.width ) {
size_of_images.width = surface_capabilities.minImageExtent.width;
} else if( size_of_images.width >
surface_capabilities.maxImageExtent.width ) {
size_of_images.width = surface_capabilities.maxImageExtent.width;
}
if( size_of_images.height < surface_capabilities.minImageExtent.height )
{
size_of_images.height = surface_capabilities.minImageExtent.height;
} else if( size_of_images.height >
surface_capabilities.maxImageExtent.height ) {
size_of_images.height = surface_capabilities.maxImageExtent.height;
}
} else {
size_of_images = surface_capabilities.currentExtent;
}
return true;
}
