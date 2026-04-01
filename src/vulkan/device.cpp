#include "device.h"

#include "descriptor_set.h"
#include "object_destroyer.h"
#include "pipeline.h"
#include "pipeline_layout.h"
#include "render_target.h"
#include "rendering_driver.h"
#include "sampler.h"
#include "shader_module.h"
#include "surface.h"
#include "swap_chain.h"
#include "texture.h"
#include "vulkan_logger.h"
#include "wrappers.h"

#include <array>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Device class implementation

Device::Device(RenderingDriver& instance, PhysicalDevice& physical_device)
    : instance_(instance), physical_device_(physical_device), graphics_queue_(*this), compute_queue_(*this),
      transfer_queue_(*this), present_queue_(*this) {}

Device::~Device() {
    ObjectDestroyer<VkDescriptorPool>::destroy(device_, descriptor_pool_);
    for (auto& kit : transfer_kits_) { ObjectDestroyer<VkFence>::destroy(device_, kit.fence); }
    transfer_kits_.clear();
    vmaDestroyAllocator(vma_allocator_);
    ObjectDestroyer<VkDevice>::destroy(device_);
}

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
        logError(LOG_VK "couldn't create logical device: {}", result);
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

    const VmaVulkanFunctions vulkan_functions{
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
    };

    const VmaAllocatorCreateInfo allocator_create_info{
        .flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT,
        .physicalDevice = ~physical_device_,
        .device = device_,
        .pVulkanFunctions = &vulkan_functions,
        .instance = ~instance_,
        .vulkanApiVersion = VK_API_VERSION_1_2,
    };

    result = vmaCreateAllocator(&allocator_create_info, &vma_allocator_);
    if (result != VK_SUCCESS || vma_allocator_ == VK_NULL_HANDLE) {
        logError(LOG_VK "couldn't create VMA allocator: {}", result);
        return false;
    }

    if (!graphics_queue_.create()) { return false; }
    if (!compute_queue_.create()) { return false; }
    if (!transfer_queue_.create()) { return false; }
    if (!present_queue_.create()) { return false; }

    transfer_kits_.reserve(TRANSFER_KIT_COUNT);
    for (std::uint32_t n = 0; n < TRANSFER_KIT_COUNT; ++n) {
        auto& kit = transfer_kits_.emplace_back(*this);
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        if (!transfer_queue_.obtainCommandBuffer(command_buffer)) { return false; }
        kit.command_buffer = CommandBuffer::wrap(command_buffer);
        if (!createFence(true, kit.fence)) { return false; }
    }

    if (!createDescriptorPool(8,
                              std::array{
                                  VkDescriptorPoolSize{
                                      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      .descriptorCount = 4,
                                  },
                                  VkDescriptorPoolSize{
                                      .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      .descriptorCount = 4,
                                  },
                              },
                              descriptor_pool_)) {
        return false;
    }

    shader_modules_.reserve(16);
    pipeline_layouts_.reserve(16);
    pipelines_.reserve(16);
    buffers_.reserve(16);
    textures_.reserve(16);
    samplers_.reserve(16);
    descriptor_sets_.reserve(16);

    return true;
}

void Device::finalize() {
    if (device_ == VK_NULL_HANDLE) { return; }
    waitDevice();
    descriptor_sets_.clear();
    samplers_.clear();
    textures_.clear();
    buffers_.clear();
    pipelines_.clear();
    pipeline_layouts_.clear();
    shader_modules_.clear();
    graphics_queue_.destroy();
    compute_queue_.destroy();
    transfer_queue_.destroy();
    present_queue_.destroy();
}

bool Device::waitDevice() {
    VkResult result = vkDeviceWaitIdle(device_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "waiting for device failed: {}", result);
        return false;
    }
    return true;
}

bool Device::createSemaphore(VkSemaphore& semaphore) {
    const VkSemaphoreCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkResult result = vkCreateSemaphore(device_, &create_info, nullptr, &semaphore);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create semaphore: {}", result);
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
        logError(LOG_VK "couldn't create fence: {}", result);
        return false;
    }
    return true;
}

bool Device::waitForFences(std::span<const VkFence> fences, VkBool32 wait_for_all, std::uint64_t timeout) {
    VkResult result = vkWaitForFences(device_, std::uint32_t(fences.size()), fences.data(), wait_for_all, timeout);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "waiting for fences failed: {}", result);
        return false;
    }
    return true;
}

bool Device::resetFences(std::span<const VkFence> fences) {
    VkResult result = vkResetFences(device_, std::uint32_t(fences.size()), fences.data());
    if (result != VK_SUCCESS) {
        logError(LOG_VK "error occurred when tried to reset fences: {}", result);
        return false;
    }
    return true;
}

bool Device::obtainDescriptorSet(VkDescriptorSetLayout descriptor_set_layout, VkDescriptorSet& descriptor_set) {
    const VkDescriptorSetAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptor_set_layout,
    };

    VkResult result = vkAllocateDescriptorSets(device_, &allocate_info, &descriptor_set);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't allocate descriptor sets: {}", result);
        return false;
    }

    return true;
}

void Device::releaseDescriptorSet(VkDescriptorSet descriptor_set) {
    vkFreeDescriptorSets(device_, descriptor_pool_, 1, &descriptor_set);
}

bool Device::updateBuffer(const void* data, VkDeviceSize data_size, VkBuffer dst, VkDeviceSize dst_offset,
                          VkAccessFlags dst_current_access, VkAccessFlags dst_new_access,
                          VkPipelineStageFlags dst_generating_stages, VkPipelineStageFlags dst_consuming_stages,
                          std::span<const VkSemaphore> signal_semaphores) {
    if (++current_transfer_kit_ == TRANSFER_KIT_COUNT) { current_transfer_kit_ = 0; }
    auto& kit = transfer_kits_[current_transfer_kit_];

    if (!waitForFences(std::array{kit.fence}, VK_FALSE, 500000000)) { return false; }

    if (kit.staging_buffer.getSize() < data_size) {
        if (!kit.staging_buffer.create(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true)) { return false; }
    }

    VkResult result = vmaCopyMemoryToAllocation(vma_allocator_, data, kit.staging_buffer.getAllocation(), 0, data_size);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't copy to host visible memory: {}", result);
        return false;
    }

    if (!kit.command_buffer.beginCommandBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr)) { return false; }

    kit.command_buffer.setBufferMemoryBarrier(dst_generating_stages, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                              std::array{
                                                  Wrapper<VkBufferMemoryBarrier>::unwrap({
                                                      .buffer = dst,
                                                      .current_access = dst_current_access,
                                                      .new_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                                                      .current_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                      .new_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                  }),
                                              });

    kit.command_buffer.copyBuffer(~kit.staging_buffer, dst,
                                  std::array{
                                      VkBufferCopy{.dstOffset = dst_offset, .size = data_size},
                                  });

    kit.command_buffer.setBufferMemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, dst_consuming_stages,
                                              std::array{
                                                  Wrapper<VkBufferMemoryBarrier>::unwrap({
                                                      .buffer = dst,
                                                      .current_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                                                      .new_access = dst_new_access,
                                                      .current_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                      .new_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                  }),
                                              });

    if (!kit.command_buffer.endCommandBuffer()) { return false; }

    if (!resetFences(std::array{kit.fence})) { return false; }

    if (!transfer_queue_.submitCommandBuffers({}, std::array{~kit.command_buffer}, signal_semaphores, kit.fence)) {
        return false;
    }

    return true;
}

bool Device::updateImage(const void* data, VkDeviceSize data_size, VkImage dst,
                         VkImageSubresourceLayers dst_subresource, VkOffset3D dst_offset, VkExtent3D dst_extent,
                         VkImageLayout dst_current_layout, VkImageLayout dst_new_layout,
                         VkAccessFlags dst_current_access, VkAccessFlags dst_new_access, VkImageAspectFlags dst_aspect,
                         VkPipelineStageFlags dst_generating_stages, VkPipelineStageFlags dst_consuming_stages,
                         std::span<const VkSemaphore> signal_semaphores) {
    if (++current_transfer_kit_ == TRANSFER_KIT_COUNT) { current_transfer_kit_ = 0; }
    auto& kit = transfer_kits_[current_transfer_kit_];

    if (!waitForFences(std::array{kit.fence}, VK_FALSE, 500000000)) { return false; }

    if (kit.staging_buffer.getSize() < data_size) {
        if (!kit.staging_buffer.create(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true)) { return false; }
    }

    VkResult result = vmaCopyMemoryToAllocation(vma_allocator_, data, kit.staging_buffer.getAllocation(), 0, data_size);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't copy to host visible memory: {}", result);
        return false;
    }

    if (!kit.command_buffer.beginCommandBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr)) { return false; }

    kit.command_buffer.setImageMemoryBarrier(dst_generating_stages, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                             std::array{
                                                 Wrapper<VkImageMemoryBarrier>::unwrap({
                                                     .image = dst,
                                                     .current_access = dst_current_access,
                                                     .new_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                                                     .current_layout = dst_current_layout,
                                                     .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                     .current_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                     .new_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                     .aspect = dst_aspect,
                                                 }),
                                             });

    kit.command_buffer.copyBufferToImage(~kit.staging_buffer, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         std::array{
                                             VkBufferImageCopy{
                                                 .imageSubresource = dst_subresource,
                                                 .imageOffset = dst_offset,
                                                 .imageExtent = dst_extent,
                                             },
                                         });

    kit.command_buffer.setImageMemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, dst_consuming_stages,
                                             std::array{
                                                 Wrapper<VkImageMemoryBarrier>::unwrap({
                                                     .image = dst,
                                                     .current_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                                                     .new_access = dst_new_access,
                                                     .current_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                     .new_layout = dst_new_layout,
                                                     .current_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                     .new_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                     .aspect = dst_aspect,
                                                 }),
                                             });

    if (!kit.command_buffer.endCommandBuffer()) { return false; }

    if (!resetFences(std::array{kit.fence})) { return false; }

    if (!transfer_queue_.submitCommandBuffers({}, std::array{~kit.command_buffer}, signal_semaphores, kit.fence)) {
        return false;
    }

    return true;
}

//@{ IDevice

IShaderModule* Device::createShaderModule(std::span<const std::uint32_t> source) {
    auto shader_module = std::make_unique<ShaderModule>(*this);
    if (!shader_module->create(source)) { return nullptr; }
    return shader_modules_.emplace_back(std::move(shader_module)).get();
}

IPipelineLayout* Device::createPipelineLayout(const uxs::db::value& config) {
    auto pipeline_layout = std::make_unique<PipelineLayout>(*this);
    if (!pipeline_layout->create(config)) { return nullptr; }
    return pipeline_layouts_.emplace_back(std::move(pipeline_layout)).get();
}

IPipeline* Device::createPipeline(IRenderTarget& render_target, IPipelineLayout& pipeline_layout,
                                  std::span<IShaderModule* const> shader_modules, const uxs::db::value& config) {
    auto pipeline = std::make_unique<Pipeline>(*this, static_cast<PipelineLayout&>(pipeline_layout));
    if (!pipeline->create(static_cast<RenderTarget&>(render_target), shader_modules, config)) { return nullptr; }
    return pipelines_.emplace_back(std::move(pipeline)).get();
}

IBuffer* Device::createBuffer(std::size_t size, BufferType type) {
    constexpr std::array usage{
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    };
    auto buffer = std::make_unique<Buffer>(*this);
    if (!buffer->create(size, usage[unsigned(type)] | VK_BUFFER_USAGE_TRANSFER_DST_BIT)) { return nullptr; }
    return buffers_.emplace_back(std::move(buffer)).get();
}

ITexture* Device::createTexture(Extent3u extent) {
    auto texture = std::make_unique<Texture>(*this);
    if (!texture->create(VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
                         {.width = extent.width, .height = extent.height, .depth = extent.depth}, 1, 1,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false, VK_IMAGE_VIEW_TYPE_2D)) {
        return nullptr;
    }
    return textures_.emplace_back(std::move(texture)).get();
}

ISampler* Device::createSampler() {
    auto sampler = std::make_unique<Sampler>(*this);
    if (!sampler->create(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST,
                         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f, false, 1.0f, false, VK_COMPARE_OP_ALWAYS, 0.0f,
                         1.0f, VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK, false)) {
        return nullptr;
    }
    return samplers_.emplace_back(std::move(sampler)).get();
}

IDescriptorSet* Device::createDescriptorSet(IPipelineLayout& pipeline_layout) {
    auto descriptor_set = std::make_unique<DescriptorSet>(*this, static_cast<PipelineLayout&>(pipeline_layout));
    if (!descriptor_set->create()) { return nullptr; }
    return descriptor_sets_.emplace_back(std::move(descriptor_set)).get();
}

//@}

bool Device::createDescriptorPool(std::uint32_t max_sets_count, std::span<const VkDescriptorPoolSize> descriptor_types,
                                  VkDescriptorPool& descriptor_pool) {
    const VkDescriptorPoolCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = max_sets_count,
        .poolSizeCount = std::uint32_t(descriptor_types.size()),
        .pPoolSizes = descriptor_types.data(),
    };

    VkResult result = vkCreateDescriptorPool(device_, &create_info, nullptr, &descriptor_pool);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create descriptor pool: {}", result);
        return false;
    }

    return true;
}
