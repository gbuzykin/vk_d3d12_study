#include "device.h"

#include "rendering_driver.h"

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

    std::uint32_t compute_queue_family_index = 0;

    if (caps.needs_compute) {
        compute_queue_family_index = physical_device_.findSuitableQueueFamily(VK_QUEUE_COMPUTE_BIT);
        if (compute_queue_family_index == INVALID_UINT32_VALUE) {
            logError("couldn't obtain compute queue family index");
            return false;
        }

        if (graphics_queue_family_index != compute_queue_family_index) {
            create_info.queue_infos.push_back({compute_queue_family_index, {1.0f}});
        }
    }

    if (!physical_device_.createLogicalDevice(create_info, device_)) { return false; }

    vkGetDeviceQueue(device_, graphics_queue_family_index, 0, &graphics_queue_);

    if (caps.needs_compute) { vkGetDeviceQueue(device_, compute_queue_family_index, 0, &compute_queue_); }

    return true;
}
