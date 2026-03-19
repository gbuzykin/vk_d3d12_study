#include "device.h"

#include "buffer.h"
#include "object_destroyer.h"
#include "pipeline.h"
#include "render_target.h"
#include "rendering_driver.h"
#include "shader_module.h"
#include "surface.h"
#include "swap_chain.h"
#include "wrappers.h"

#include "common/logger.h"

#include <algorithm>
#include <array>
#include <cstring>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Device class implementation

Device::Device(RenderingDriver& instance, PhysicalDevice& physical_device)
    : instance_(instance), physical_device_(physical_device), graphics_queue_(*this), compute_queue_(*this),
      transfer_queue_(*this), present_queue_(*this) {}

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

    const auto is_queue_family_used = [&queue_create_infos, &queue_family_count](std::uint32_t family_index) {
        return std::ranges::any_of(std::span{queue_create_infos.data(), queue_family_count},
                                   [family_index](const auto& info) { return info.queueFamilyIndex == family_index; });
    };

    const auto select_queue_family = [&queue_family_count, &is_queue_family_used,
                                      &physical_device = physical_device_](VkQueueFlags flags) {
        std::uint32_t selected_family_index = INVALID_UINT32_VALUE;
        for (std::uint32_t n = 0; n <= queue_family_count; ++n) {
            const std::uint32_t family_index = physical_device.findSuitableQueueFamily(flags, n);
            if (family_index == INVALID_UINT32_VALUE) { break; }
            selected_family_index = family_index;
            if (!is_queue_family_used(selected_family_index)) { break; }
        }
        return selected_family_index;
    };

    const auto add_queue_family = [&queue_create_infos, &queue_family_count, &is_queue_family_used](
                                      std::uint32_t family_index, std::span<const float> priorities) {
        if (!is_queue_family_used(family_index)) {
            queue_create_infos[queue_family_count++] = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = family_index,
                .queueCount = std::uint32_t(priorities.size()),
                .pQueuePriorities = priorities.data(),
            };
        }
    };

    graphics_queue_.setFamilyIndex(physical_device_.findSuitableQueueFamily(VK_QUEUE_GRAPHICS_BIT));
    if (graphics_queue_.getFamilyIndex() == INVALID_UINT32_VALUE) {
        logError(LOG_VK "couldn't obtain graphics queue family index");
        return false;
    }

    const std::array priority{1.0f};
    add_queue_family(graphics_queue_.getFamilyIndex(), priority);

    if (caps.value<bool>("needs_compute")) {
        compute_queue_.setFamilyIndex(select_queue_family(VK_QUEUE_COMPUTE_BIT));
        if (compute_queue_.getFamilyIndex() == INVALID_UINT32_VALUE) {
            logError(LOG_VK "couldn't obtain compute queue family index");
            return false;
        }
        add_queue_family(compute_queue_.getFamilyIndex(), priority);
    }

    transfer_queue_.setFamilyIndex(physical_device_.findSuitableQueueFamily(VK_QUEUE_TRANSFER_BIT));
    if (transfer_queue_.getFamilyIndex() == INVALID_UINT32_VALUE) {
        logError(LOG_VK "couldn't obtain transfer queue family index");
        return false;
    }
    add_queue_family(transfer_queue_.getFamilyIndex(), priority);

    if (transfer_queue_.getFamilyIndex() != graphics_queue_.getFamilyIndex()) {
        logError(LOG_VK "different transfer and present queue families");
        return false;
    }

    for (const auto& surface : instance_.getSurfaces()) {
        const std::uint32_t family_index = surface->getPresentQueueFamily();
        if (present_queue_.getFamilyIndex() == INVALID_UINT32_VALUE) {
            if (family_index == INVALID_UINT32_VALUE) {
                logError(LOG_VK "couldn't obtain present queue family index");
                return false;
            }
            present_queue_.setFamilyIndex(family_index);
        } else if (present_queue_.getFamilyIndex() != family_index) {
            logError(LOG_VK "inconsistent present queue families for surfaces");
            return false;
        }
    }

    if (present_queue_.getFamilyIndex() != graphics_queue_.getFamilyIndex()) {
        logError(LOG_VK "different graphics and present queue families");
        return false;
    }

    add_queue_family(present_queue_.getFamilyIndex(), priority);

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

    if (!graphics_queue_.create()) { return false; }
    if (!compute_queue_.create()) { return false; }
    if (!transfer_queue_.create()) { return false; }
    if (!present_queue_.create()) { return false; }

    transfer_command_buffer_ = CommandBuffer::wrap(transfer_queue_.obtainCommandBuffer());

    shader_modules_.reserve(16);
    pipelines_.reserve(16);
    buffers_.reserve(16);

    return true;
}

void Device::finalize() {
    if (device_ == VK_NULL_HANDLE) { return; }
    waitDevice();
    buffers_.clear();
    pipelines_.clear();
    shader_modules_.clear();
    graphics_queue_.destroy();
    compute_queue_.destroy();
    transfer_queue_.destroy();
    present_queue_.destroy();
}

bool Device::waitDevice() {
    VkResult result = vkDeviceWaitIdle(device_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "waiting for device failed");
        return false;
    }
    return true;
}

bool Device::createSemaphore(VkSemaphore& semaphore) {
    const VkSemaphoreCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkResult result = vkCreateSemaphore(device_, &create_info, nullptr, &semaphore);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create a semaphore");
        return false;
    }
    return true;
}

bool Device::createFence(bool signaled, VkFence& fence) {
    const VkFenceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u,
    };

    VkResult result = vkCreateFence(device_, &create_info, nullptr, &fence);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create a fence");
        return false;
    }
    return true;
}

bool Device::waitForFences(std::span<const VkFence> fences, VkBool32 wait_for_all, std::uint64_t timeout) {
    VkResult result = vkWaitForFences(device_, std::uint32_t(fences.size()), fences.data(), wait_for_all, timeout);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "waiting for fence failed");
        return false;
    }
    return true;
}

bool Device::resetFences(std::span<const VkFence> fences) {
    VkResult result = vkResetFences(device_, std::uint32_t(fences.size()), fences.data());
    if (result != VK_SUCCESS) {
        logError(LOG_VK "error occurred when tried to reset fences");
        return false;
    }
    return true;
}

bool Device::writeToDeviceLocalMemory(VkDeviceSize data_size, const void* data, VkBuffer dst, VkDeviceSize dst_offset,
                                      VkAccessFlags dst_current_access, VkAccessFlags dst_new_access,
                                      VkPipelineStageFlags dst_generating_stages,
                                      VkPipelineStageFlags dst_consuming_stages,
                                      std::span<const VkSemaphore> signal_semaphores) {
    const auto non_coherent_atom_size = getPhysicalDevice().getProperties().limits.nonCoherentAtomSize;

    Buffer staging_buffer(*this);
    if (!staging_buffer.create((data_size + non_coherent_atom_size - 1) & ~(non_coherent_atom_size - 1),
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
        return false;
    }

    if (!writeToHostVisibleMemory(staging_buffer.getMemoryObject(), 0, data_size, data)) { return false; }

    if (!transfer_command_buffer_.beginCommandBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr)) {
        return false;
    }

    transfer_command_buffer_.setBufferMemoryBarrier(dst_generating_stages, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                    std::array{
                                                        Wrapper<VkBufferMemoryBarrier>::unwrap({
                                                            .buffer = dst,
                                                            .current_access = dst_current_access,
                                                            .new_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                                                            .current_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                            .new_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                        }),
                                                    });

    transfer_command_buffer_.copyDataBetweenBuffers(
        ~staging_buffer, dst,
        std::array{
            VkBufferCopy{.srcOffset = 0, .dstOffset = dst_offset, .size = data_size},
        });

    transfer_command_buffer_.setBufferMemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, dst_consuming_stages,
                                                    std::array{
                                                        Wrapper<VkBufferMemoryBarrier>::unwrap({
                                                            .buffer = dst,
                                                            .current_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                                                            .new_access = dst_new_access,
                                                            .current_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                            .new_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                        }),
                                                    });

    if (!transfer_command_buffer_.endCommandBuffer()) { return false; }

    ObjectDestroyer<VkFence> fence(device_);
    if (!createFence(false, ~fence)) { return false; }

    if (!transfer_queue_.submitCommandBuffers({}, std::array{~transfer_command_buffer_}, signal_semaphores, ~fence)) {
        return false;
    }

    if (!waitForFences(std::array{~fence}, VK_FALSE, 500000000)) { return false; }

    return true;
}

//@{ IDevice

IShaderModule* Device::createShaderModule(std::span<const std::uint32_t> source) {
    auto shader_module = std::make_unique<ShaderModule>(*this);
    if (!shader_module->create(source)) { return nullptr; }
    return shader_modules_.emplace_back(std::move(shader_module)).get();
}

IPipeline* Device::createPipeline(IRenderTarget& render_target, std::span<IShaderModule* const> shader_modules,
                                  const uxs::db::value& config) {
    auto pipeline = std::make_unique<Pipeline>(*this);
    if (!pipeline->create(static_cast<RenderTarget&>(render_target), shader_modules, config)) { return nullptr; }
    return pipelines_.emplace_back(std::move(pipeline)).get();
}

IBuffer* Device::createBuffer(std::size_t size) {
    auto buffer = std::make_unique<Buffer>(*this);
    if (!buffer->create(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
        return nullptr;
    }
    return buffers_.emplace_back(std::move(buffer)).get();
}

//@}

bool MappedMemory::map(VkDeviceSize offset, VkDeviceSize data_size) {
    VkResult result = vkMapMemory(device_, memory_object_, offset, data_size, 0, &ptr_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't map memory object");
        return false;
    }
    return true;
}

bool Device::writeToHostVisibleMemory(VkDeviceMemory memory_object, VkDeviceSize offset, VkDeviceSize data_size,
                                      const void* data) {
    const auto non_coherent_atom_size = getPhysicalDevice().getProperties().limits.nonCoherentAtomSize;

    const auto data_top = (offset + data_size + non_coherent_atom_size - 1) & ~(non_coherent_atom_size - 1);

    MappedMemory mapped_memory(device_, memory_object);
    if (!mapped_memory.map(offset, data_top - offset)) { return false; }

    std::memcpy(mapped_memory.ptr(), data, std::size_t(data_size));

    const std::array memory_ranges{VkMappedMemoryRange{
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = memory_object,
        .offset = offset,
        .size = VK_WHOLE_SIZE,
    }};

    VkResult result = vkFlushMappedMemoryRanges(device_, std::uint32_t(memory_ranges.size()), memory_ranges.data());
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't flush mapped memory");
        return false;
    }

    return true;
}
