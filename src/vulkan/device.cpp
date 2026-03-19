#include "device.h"

#include "object_destroyer.h"
#include "rendering_driver.h"

#include "common/logger.h"

#include <array>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Device class implementation

Device::Device(RenderingDriver& instance, PhysicalDevice& physical_device)
    : instance_(instance), physical_device_(physical_device) {}

Device::~Device() { ObjectDestroyer<VkDevice>::destroy(device_); }

bool Device::create(const uxs::db::value& caps) {
    std::vector<const char*> device_extensions;
    device_extensions.reserve(32);
    device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    device_extensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);

    const char* portability_subset_extension_name = "VK_KHR_portability_subset";
    if (physical_device_.isExtensionSupported(portability_subset_extension_name)) {
        device_extensions.push_back("VK_KHR_portability_subset");
    }

    for (const char* extension : device_extensions) {
        if (!physical_device_.isExtensionSupported(extension)) {
            logError(LOG_VK "device extension '{}' is not supported", extension);
            return false;
        }
    }

    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT features_ext{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .extendedDynamicState = VK_TRUE,
    };

    VkPhysicalDeviceFeatures2 features2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features_ext,
        .features = physical_device_.getFeatures(),
    };

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

    if (caps.value<bool>("needs_compute")) {
        compute_queue_.family_index = physical_device_.findSuitableQueueFamily(VK_QUEUE_COMPUTE_BIT);
        if (compute_queue_.family_index == INVALID_UINT32_VALUE) {
            logError(LOG_VK "couldn't obtain compute queue family index");
            return false;
        }

        add_queue_create_info(compute_queue_.family_index, priority);
    }

    const VkDeviceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features2,
        .queueCreateInfoCount = queue_family_count,
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledExtensionCount = std::uint32_t(device_extensions.size()),
        .ppEnabledExtensionNames = device_extensions.data(),
    };

    VkResult result = vkCreateDevice(~physical_device_, &create_info, nullptr, &device_);
    if (result != VK_SUCCESS || device_ == VK_NULL_HANDLE) {
        logError(LOG_VK "couldn't create logical device");
        return false;
    }

    const auto is_extension_enabled = [&device_extensions](const char* extension) {
        return std::ranges::any_of(device_extensions, [extension](const char* enabled_extension) {
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

    for (const char* extension : device_extensions) {
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
