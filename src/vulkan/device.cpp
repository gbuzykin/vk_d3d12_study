#include "device.h"

#include "descriptor_set.h"
#include "pipeline.h"
#include "render_target.h"
#include "rendering_driver.h"
#include "sampler.h"
#include "shader_module.h"
#include "surface.h"
#include "swap_chain.h"
#include "texture.h"
#include "vulkan_logger.h"
#include "wrappers.h"

#include "rel/tables.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Device class implementation

Device::Device(RenderingDriver& instance, PhysicalDevice& physical_device)
    : instance_(util::not_null(&instance)), physical_device_(physical_device) {}

Device::~Device() {
    for (auto& kit : transfer_kits_) {
        waitForFences(std::array{kit.fence}, VK_FALSE, FINISH_TRANSFER_TIMEOUT);
        vmaDestroyBuffer(allocator_, kit.staging_buffer.handle, kit.staging_buffer.allocation);
        vkDestroyFence(kit.fence, nullptr);
    }
    graphics_queue_.destroy();
    compute_queue_.destroy();
    transfer_queue_.destroy();
    for (auto* surface : instance_->getSurfaces()) { surface->getPresentQueue().destroy(); }
    vmaDestroyAllocator(allocator_);
    vkDestroyDevice(nullptr);
}

bool Device::create(const uxs::db::value& caps) {
    std::vector<const char*> device_extensions;
    device_extensions.reserve(32);
    device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME);
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

    VkPhysicalDeviceImagelessFramebufferFeatures imageless_framebuffer_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES,
        .imagelessFramebuffer = VK_TRUE,
    };

    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT dynamic_state_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = &imageless_framebuffer_features,
        .extendedDynamicState = VK_TRUE,
    };

    VkPhysicalDeviceFeatures2 features2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &dynamic_state_features,
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

    for (auto* surface : instance_->getSurfaces()) {
        const std::uint32_t family_index = surface->getPresentQueueFamily();
        if (family_index == INVALID_UINT32_VALUE) {
            logError(LOG_VK "couldn't obtain present queue family index");
            return false;
        }
        surface->getPresentQueue().setFamilyIndex(family_index);
        add_queue_family(family_index, priority);
    }

    const VkDeviceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features2,
        .queueCreateInfoCount = queue_family_count,
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledExtensionCount = std::uint32_t(device_extensions.size()),
        .ppEnabledExtensionNames = device_extensions.data(),
    };

    VkResult result = physical_device_.vkCreateDevice(&create_info, nullptr, &device_);
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
    vk_funcs_.name = (PFN_##name)instance_->getVkFuncs().vkGetDeviceProcAddr(device_, #name); \
    if (!vk_funcs_.name) { \
        logError(LOG_VK "couldn't obtain device-level Vulkan function '{}'", #name); \
        return false; \
    }

#define DEVICE_LEVEL_VK_FUNCTION_FROM_EXTENSION(name, extension) \
    if (is_extension_enabled(extension)) { \
        vk_funcs_.name = (PFN_##name)instance_->getVkFuncs().vkGetDeviceProcAddr(device_, #name); \
        if (!vk_funcs_.name) { \
            logError(LOG_VK "couldn't obtain device-level Vulkan function '{}'", #name); \
            return false; \
        } \
    } else { \
        logError(LOG_VK "device extension '{}' is not enabled", extension); \
        return false; \
    }

#define DEVICE_LEVEL_VK_FUNCTION_QUEUE(name) DEVICE_LEVEL_VK_FUNCTION(name)
#define DEVICE_LEVEL_VK_FUNCTION_CMD(name)   DEVICE_LEVEL_VK_FUNCTION(name)
#define DEVICE_LEVEL_VK_FUNCTION_FROM_EXTENSION_QUEUE(name, extension) \
    DEVICE_LEVEL_VK_FUNCTION_FROM_EXTENSION(name, extension)
#define DEVICE_LEVEL_VK_FUNCTION_FROM_EXTENSION_CMD(name, extension) \
    DEVICE_LEVEL_VK_FUNCTION_FROM_EXTENSION(name, extension)
#include "vulkan_function_list.inl"

    const VmaVulkanFunctions vulkan_functions{
        .vkGetInstanceProcAddr = instance_->getVkFuncs().vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = instance_->getVkFuncs().vkGetDeviceProcAddr,
    };

    const VmaAllocatorCreateInfo allocator_create_info{
        .flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT,
        .physicalDevice = physical_device_.getHandle(),
        .device = device_,
        .pVulkanFunctions = &vulkan_functions,
        .instance = instance_->getHandle(),
        .vulkanApiVersion = VK_API_VERSION_1_2,
    };

    result = vmaCreateAllocator(&allocator_create_info, &allocator_);
    if (result != VK_SUCCESS || allocator_ == VK_NULL_HANDLE) {
        logError(LOG_VK "couldn't create VMA allocator: {}", result);
        return false;
    }

    if (!graphics_queue_.create(*this, 0)) { return false; }
    if (!compute_queue_.create(*this, 0)) { return false; }
    if (!transfer_queue_.create(*this, TRANSFER_KIT_COUNT)) { return false; }
    for (auto* surface : instance_->getSurfaces()) {
        if (!surface->getPresentQueue().create(*this, 0)) { return false; }
    }

    transfer_kits_.resize(TRANSFER_KIT_COUNT);
    for (std::uint32_t n = 0; n < TRANSFER_KIT_COUNT; ++n) {
        if (!transfer_queue_.obtainCommandBuffer(n, transfer_kits_[n].command_buffer)) { return false; }
        if (!createFence(true, transfer_kits_[n].fence)) { return false; }
    }

    return true;
}

bool Device::createSemaphore(VkSemaphore& semaphore) {
    VkResult result = vkCreateSemaphore(
        constAddressOf(VkSemaphoreCreateInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}), nullptr, &semaphore);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create semaphore: {}", result);
        return false;
    }
    return true;
}

bool Device::createFence(bool signaled, VkFence& fence) {
    VkResult result = vkCreateFence(constAddressOf(VkFenceCreateInfo{
                                        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                        .flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u,
                                    }),
                                    nullptr, &fence);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create fence: {}", result);
        return false;
    }
    return true;
}

bool Device::waitForFences(std::span<const VkFence> fences, VkBool32 wait_for_all, std::uint64_t timeout) {
    VkResult result = vkWaitForFences(std::uint32_t(fences.size()), fences.data(), wait_for_all, timeout);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "waiting for fences failed: {}", result);
        return false;
    }
    return true;
}

bool Device::resetFences(std::span<const VkFence> fences) {
    VkResult result = vkResetFences(std::uint32_t(fences.size()), fences.data());
    if (result != VK_SUCCESS) {
        logError(LOG_VK "error occurred when tried to reset fences: {}", result);
        return false;
    }
    return true;
}

bool Device::updateBuffer(std::span<const std::uint8_t> data, VkBuffer dst, VkDeviceSize offset,
                          VkPipelineStageFlags generating_stages, VkPipelineStageFlags consuming_stages,
                          VkAccessFlags current_access, VkAccessFlags new_access,
                          std::span<const VkSemaphore> signal_semaphores) {
    auto& kit = transfer_kits_[current_transfer_kit_];

    if (!waitForFences(std::array{kit.fence}, VK_FALSE, FINISH_TRANSFER_TIMEOUT)) { return false; }

    if (kit.staging_buffer.size < data.size()) {
        if (!createStagingBuffer(VkDeviceSize(data.size()), kit.staging_buffer)) { return false; }
    }

    VkResult result = vmaCopyMemoryToAllocation(allocator_, data.data(), kit.staging_buffer.allocation, 0,
                                                VkDeviceSize(data.size()));
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't copy to host visible memory: {}", result);
        return false;
    }

    if (!transfer_queue_.resetCommandPool(current_transfer_kit_)) { return false; }

    if (!kit.command_buffer.beginCommandBuffer(0, nullptr)) { return false; }

    kit.command_buffer.setBufferMemoryBarrier(generating_stages, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                              std::array{
                                                  Wrapper<VkBufferMemoryBarrier>::unwrap({
                                                      .buffer = dst,
                                                      .current_access = current_access,
                                                      .new_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                                                      .current_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                      .new_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                  }),
                                              });

    kit.command_buffer.copyBuffer(kit.staging_buffer.handle, dst,
                                  std::array{
                                      VkBufferCopy{.dstOffset = offset, .size = VkDeviceSize(data.size())},
                                  });

    kit.command_buffer.setBufferMemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, consuming_stages,
                                              std::array{
                                                  Wrapper<VkBufferMemoryBarrier>::unwrap({
                                                      .buffer = dst,
                                                      .current_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                                                      .new_access = new_access,
                                                      .current_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                      .new_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                  }),
                                              });

    if (!kit.command_buffer.endCommandBuffer()) { return false; }

    if (!resetFences(std::array{kit.fence})) { return false; }

    if (!transfer_queue_.submitCommandBuffers({}, std::array{kit.command_buffer.getHandle()}, signal_semaphores,
                                              kit.fence)) {
        return false;
    }

    if (++current_transfer_kit_ == TRANSFER_KIT_COUNT) { current_transfer_kit_ = 0; }
    return true;
}

bool Device::updateImage(const std::uint8_t* data, VkImage dst, Format format, std::uint32_t first_subresource,
                         std::span<const UpdateTextureDesc> update_subresource_descs,
                         VkPipelineStageFlags generating_stages, VkPipelineStageFlags consuming_stages,
                         VkAccessFlags current_access, VkAccessFlags new_access, VkImageLayout current_layout,
                         VkImageLayout new_layout, VkImageAspectFlags aspect,
                         std::span<const VkSemaphore> signal_semaphores) {
    auto& kit = transfer_kits_[current_transfer_kit_];

    if (!waitForFences(std::array{kit.fence}, VK_FALSE, FINISH_TRANSFER_TIMEOUT)) { return false; }

    const std::uint32_t bytes_per_pixel = TBL_FORMAT_SIZE[unsigned(format)];

    auto size_of_subresource = [bytes_per_pixel](const auto& desc) {
        return (desc.buffer_row_size ?
                    std::size_t(desc.buffer_row_size) * desc.buffer_row_count :
                    std::size_t(desc.image_extent.width * bytes_per_pixel) * desc.image_extent.height) *
               desc.image_extent.depth;
    };

    std::size_t buf_offset = 0;
    std::size_t total_buf_size = 0;

    for (const auto& desc : update_subresource_descs) {
        const std::size_t buf_size = size_of_subresource(desc);
        total_buf_size = std::max((desc.buffer_offset ? desc.buffer_offset : buf_offset) + buf_size, total_buf_size);
        buf_offset += buf_size;
    }

    if (kit.staging_buffer.size < total_buf_size) {
        if (!createStagingBuffer(VkDeviceSize(total_buf_size), kit.staging_buffer)) { return false; }
    }

    VkResult result = vmaCopyMemoryToAllocation(allocator_, data, kit.staging_buffer.allocation, 0,
                                                VkDeviceSize(total_buf_size));
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't copy to host visible memory: {}", result);
        return false;
    }

    if (!transfer_queue_.resetCommandPool(current_transfer_kit_)) { return false; }

    if (!kit.command_buffer.beginCommandBuffer(0, nullptr)) { return false; }

    kit.command_buffer.setImageMemoryBarrier(generating_stages, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                             std::array{
                                                 Wrapper<VkImageMemoryBarrier>::unwrap({
                                                     .image = dst,
                                                     .current_access = current_access,
                                                     .new_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                                                     .current_layout = current_layout,
                                                     .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                     .current_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                     .new_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                     .aspect = aspect,
                                                 }),
                                             });

    buf_offset = 0;
    for (std::uint32_t n = 0; n < std::uint32_t(update_subresource_descs.size()); ++n) {
        const auto& desc = update_subresource_descs[n];
        kit.command_buffer.copyBufferToImage(
            kit.staging_buffer.handle, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            std::array{
                VkBufferImageCopy{
                    .bufferOffset = desc.buffer_offset ? desc.buffer_offset : buf_offset,
                    .bufferRowLength = desc.buffer_row_size,
                    .bufferImageHeight = desc.buffer_row_count,
                    .imageSubresource =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .mipLevel = first_subresource + n,
                            .baseArrayLayer = 0,
                            .layerCount = 1,
                        },
                    .imageOffset = {.x = desc.image_offset.x, .y = desc.image_offset.y, .z = desc.image_offset.z},
                    .imageExtent = {.width = desc.image_extent.width,
                                    .height = desc.image_extent.height,
                                    .depth = desc.image_extent.depth},
                },
            });
        buf_offset += size_of_subresource(desc);
    }

    kit.command_buffer.setImageMemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, consuming_stages,
                                             std::array{
                                                 Wrapper<VkImageMemoryBarrier>::unwrap({
                                                     .image = dst,
                                                     .current_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                                                     .new_access = new_access,
                                                     .current_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                     .new_layout = new_layout,
                                                     .current_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                     .new_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                     .aspect = aspect,
                                                 }),
                                             });

    if (!kit.command_buffer.endCommandBuffer()) { return false; }

    if (!resetFences(std::array{kit.fence})) { return false; }

    if (!transfer_queue_.submitCommandBuffers({}, std::array{kit.command_buffer.getHandle()}, signal_semaphores,
                                              kit.fence)) {
        return false;
    }

    if (++current_transfer_kit_ == TRANSFER_KIT_COUNT) { current_transfer_kit_ = 0; }
    return true;
}

//@{ IDevice

bool Device::waitDevice() {
    VkResult result = vkDeviceWaitIdle();
    if (result != VK_SUCCESS) {
        logError(LOG_VK "waiting for device failed: {}", result);
        return false;
    }
    return true;
}

util::ref_ptr<ISwapChain> Device::createSwapChain(ISurface& surface, const uxs::db::value& opts) {
    auto swap_chain = util::make_new<SwapChain>(*this, static_cast<Surface&>(surface));
    if (!swap_chain->create(opts)) { return nullptr; }
    return std::move(swap_chain);
}

util::ref_ptr<IShaderModule> Device::createShaderModule(DataBlob bytecode) {
    auto shader_module = util::make_new<ShaderModule>(*this);
    if (!shader_module->create(bytecode)) { return nullptr; }
    return std::move(shader_module);
}

util::ref_ptr<IPipelineLayout> Device::createPipelineLayout(const uxs::db::value& config) {
    auto pipeline_layout = util::make_new<PipelineLayout>(*this);
    if (!pipeline_layout->create(config)) { return nullptr; }
    return std::move(pipeline_layout);
}

util::ref_ptr<IPipeline> Device::createPipeline(IRenderTarget& render_target, IPipelineLayout& pipeline_layout,
                                                std::span<IShaderModule* const> shader_modules,
                                                const uxs::db::value& config) {
    auto pipeline = util::make_new<Pipeline>(*this, static_cast<RenderTarget&>(render_target),
                                             static_cast<PipelineLayout&>(pipeline_layout));
    if (!pipeline->create(shader_modules, config)) { return nullptr; }
    return std::move(pipeline);
}

util::ref_ptr<IBuffer> Device::createBuffer(BufferType type, std::uint64_t size) {
    auto buffer = util::make_new<Buffer>(*this);
    if (!buffer->create(type, VkDeviceSize(size))) { return nullptr; }
    return std::move(buffer);
}

util::ref_ptr<ITexture> Device::createTexture(const TextureDesc& desc) {
    auto texture = util::make_new<Texture>(*this);
    if (!texture->create(desc)) { return nullptr; }
    return std::move(texture);
}

util::ref_ptr<ISampler> Device::createSampler(const SamplerDesc& desc) {
    auto sampler = util::make_new<Sampler>(*this);
    if (!sampler->create(desc)) { return nullptr; }
    return std::move(sampler);
}

//@}

bool Device::createStagingBuffer(VkDeviceSize size, StagingBuffer& buffer) {
    const VkBufferCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    if (buffer.handle != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator_, buffer.handle, buffer.allocation);
        buffer.handle = VK_NULL_HANDLE;
        buffer.allocation = VK_NULL_HANDLE;
    }

    VkResult result = vmaCreateBuffer(allocator_, &create_info,
                                      constAddressOf(VmaAllocationCreateInfo{
                                          .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                                          .usage = VMA_MEMORY_USAGE_AUTO,
                                      }),
                                      &buffer.handle, &buffer.allocation, nullptr);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create buffer: {}", result);
        return false;
    }

    buffer.size = size;
    return true;
}
