#include "device.h"

#include "rendering_driver.h"
#include "surface.h"
#include "swap_chain.h"

#include "utils/logger.h"

#include <algorithm>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Device class implementation

Device::Device(RenderingDriver& instance, PhysicalDevice& physical_device)
    : instance_(instance), physical_device_(physical_device) {}

Device::~Device() {
    if (device_ != VK_NULL_HANDLE) { vkDestroyDevice(device_, nullptr); }
}

bool Device::create(const DesiredDeviceCaps& caps) {
    DeviceCreateInfo create_info;

    create_info.extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    create_info.features.geometryShader = VK_TRUE;

    create_info.queue_infos.reserve(8);

    const std::uint32_t graphics_queue_family_index = physical_device_.findSuitableQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    if (graphics_queue_family_index == INVALID_UINT32_VALUE) {
        logError(LOG_VK "couldn't obtain graphics queue family index");
        return false;
    }

    create_info.queue_infos.push_back({graphics_queue_family_index, {1.0f}});

    const auto add_family_to_create_info = [&create_info](std::uint32_t queue_family) {
        if (!std::ranges::any_of(create_info.queue_infos,
                                 [queue_family](const auto& info) { return info.family_index == queue_family; })) {
            create_info.queue_infos.push_back({queue_family, {1.0f}});
        }
    };

    std::uint32_t present_queue_family = INVALID_UINT32_VALUE;

    for (const auto& surface : instance_.getSurfaces()) {
        const std::uint32_t surface_present_queue_family = surface->getPresentQueueFamily();
        if (present_queue_family == INVALID_UINT32_VALUE) {
            present_queue_family = surface_present_queue_family;
        } else if (present_queue_family != surface_present_queue_family) {
            logError(LOG_VK "inconsistent queue families for surfaces");
            return false;
        }
    }

    add_family_to_create_info(present_queue_family);

    std::uint32_t compute_queue_family_index = INVALID_UINT32_VALUE;

    if (caps.needs_compute) {
        compute_queue_family_index = physical_device_.findSuitableQueueFamily(VK_QUEUE_COMPUTE_BIT);
        if (compute_queue_family_index == INVALID_UINT32_VALUE) {
            logError(LOG_VK "couldn't obtain compute queue family index");
            return false;
        }

        add_family_to_create_info(compute_queue_family_index);
    }

    if (!physical_device_.createLogicalDevice(create_info, device_)) { return false; }

    vkGetDeviceQueue(device_, graphics_queue_family_index, 0, &graphics_queue_);

    vkGetDeviceQueue(device_, present_queue_family, 0, &present_queue_);

    if (compute_queue_family_index != INVALID_UINT32_VALUE) {
        vkGetDeviceQueue(device_, compute_queue_family_index, 0, &compute_queue_);
    }

    return true;
}

//@{ IDevice

ISwapChain* Device::createSwapChain(ISurface& surface, const SwapChainCreateInfo& create_info) {
    return static_cast<Surface&>(surface).createSwapChain(*this, create_info);
}

bool Device::createSemaphores(std::span<SemaphoreHandle> semaphores) {
    // for (auto& semaphore : semaphores) { vkCreateSemaphore(device_, &semaphore); }

    return true;
}

void Device::destroySemaphores(std::span<const SemaphoreHandle> semaphores) {}

bool Device::createFences(std::span<FenceHandle> fences) { return false; }

void Device::destroyFences(std::span<const FenceHandle> fences) {}

//@}
