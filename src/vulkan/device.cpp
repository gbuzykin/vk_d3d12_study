#include "device.h"

#include "object_destroyer.h"
#include "rendering_driver.h"

#include "utils/logger.h"

#include <algorithm>
#include <array>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

namespace {
const std::array g_device_extensions{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};
}

// --------------------------------------------------------
// Device class implementation

Device::Device(RenderingDriver& instance, PhysicalDevice& physical_device)
    : instance_(instance), physical_device_(physical_device) {}

Device::~Device() { ObjectDestroyer<VkDevice>::destroy(device_); }

bool Device::create(const DesiredDeviceCaps& caps) {
    VkPhysicalDeviceFeatures desired_features{};

    desired_features.geometryShader = VK_TRUE;

    std::uint32_t queue_family_count = 0;
    std::array<VkDeviceQueueCreateInfo, 8> queue_create_infos;

    const auto add_queue_create_info = [&queue_create_infos, &queue_family_count](std::uint32_t family_index,
                                                                                  std::span<const float> priorities) {
        if (!std::ranges::any_of(std::span{queue_create_infos.data(), queue_family_count},
                                 [family_index](const auto& info) { return info.queueFamilyIndex == family_index; })) {
            queue_create_infos[queue_family_count++] = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = family_index,
                .queueCount = std::uint32_t(priorities.size()),
                .pQueuePriorities = priorities.data(),
            };
        }
    };

    graphics_queue_.family_index = physical_device_.findSuitableQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    if (graphics_queue_.family_index == INVALID_UINT32_VALUE) {
        logError(LOG_VK "couldn't obtain graphics queue family index");
        return false;
    }

    const std::array priority{1.0f};
    add_queue_create_info(graphics_queue_.family_index, priority);

    if (caps.needs_compute) {
        compute_queue_.family_index = physical_device_.findSuitableQueueFamily(VK_QUEUE_COMPUTE_BIT);
        if (compute_queue_.family_index == INVALID_UINT32_VALUE) {
            logError(LOG_VK "couldn't obtain compute queue family index");
            return false;
        }

        add_queue_create_info(compute_queue_.family_index, priority);
    }

    const VkDeviceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = queue_family_count,
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledExtensionCount = std::uint32_t(g_device_extensions.size()),
        .ppEnabledExtensionNames = g_device_extensions.data(),
        .pEnabledFeatures = &desired_features,
    };

    VkResult result = vkCreateDevice(~physical_device_, &create_info, nullptr, &device_);
    if (result != VK_SUCCESS || device_ == VK_NULL_HANDLE) {
        logError(LOG_VK "couldn't create logical device");
        return false;
    }

    const auto is_extension_enabled = [](const char* extension) {
        return std::ranges::any_of(g_device_extensions, [extension](const char* enabled_extension) {
            return std::string_view(extension) == std::string_view(enabled_extension);
        });
    };

#define DEVICE_LEVEL_VK_FUNCTION(name) \
    name = (PFN_##name)vkGetDeviceProcAddr(device_, #name); \
    if (!name) { \
        logError(LOG_VK "couldn't obtain device-level Vulkan function '{}'", #name); \
        return false; \
    }

#define DEVICE_LEVEL_VK_FUNCTION_FROM_EXTENSION(name, extension) \
    if (is_extension_enabled(extension)) { \
        name = (PFN_##name)vkGetDeviceProcAddr(device_, #name); \
        if (!name) { \
            logError(LOG_VK "couldn't obtain instance-level Vulkan function '{}'", #name); \
            return false; \
        } \
    } else { \
        logError(LOG_VK "device extension '{}' is not enabled", extension); \
        return false; \
    }

#include "vulkan_function_list.inl"

    vkGetDeviceQueue(device_, graphics_queue_.family_index, 0, &graphics_queue_.handle);

    if (compute_queue_.family_index != INVALID_UINT32_VALUE) {
        vkGetDeviceQueue(device_, compute_queue_.family_index, 0, &compute_queue_.handle);
    }

    for (const char* extension : g_device_extensions) {
        if (!physical_device_.isExtensionSupported(extension)) {
            logError(LOG_VK "device extension '{}' is not supported", extension);
            return false;
        }
    }

    return true;
}

bool Device::waitDevice() {
    VkResult result = vkDeviceWaitIdle(device_);
    if (result != VK_SUCCESS) {
        logError("waiting on a device failed");
        return false;
    }
    return true;
}

void Device::finalize() {
    if (device_ != VK_NULL_HANDLE) { waitDevice(); }
}
