#include "device.h"

#include "object_destroyer.h"
#include "rendering_driver.h"
#include "surface.h"
#include "swap_chain.h"
#include "wrappers.h"

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

Device::~Device() {
    ObjectDestroyer<VkCommandPool>::destroy(device_, command_pool_);
    ObjectDestroyer<VkRenderPass>::destroy(device_, render_pass_);
    ObjectDestroyer<VkSemaphore>::destroy(device_, sem_image_acquired_);
    ObjectDestroyer<VkSemaphore>::destroy(device_, sem_ready_to_present_);
    ObjectDestroyer<VkFence>::destroy(device_, fence_drawing_);
    ObjectDestroyer<VkDevice>::destroy(device_);
}

bool Device::create(const DesiredDeviceCaps& caps) {
    for (const char* extension : g_device_extensions) {
        if (!physical_device_.isExtensionSupported(extension)) {
            logError(LOG_VK "device extension '{}' is not supported", extension);
            return false;
        }
    }

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

    graphics_queue_ = DevQueue{physical_device_.findSuitableQueueFamily(VK_QUEUE_GRAPHICS_BIT)};
    if (graphics_queue_.getFamilyIndex() == INVALID_UINT32_VALUE) {
        logError(LOG_VK "couldn't obtain graphics queue family index");
        return false;
    }

    const std::array priority{1.0f};
    add_queue_create_info(graphics_queue_.getFamilyIndex(), priority);

    for (const auto& surface : instance_.getSurfaces()) {
        const std::uint32_t present_queue_family_index = surface->getPresentQueueFamily();
        if (present_queue_.getFamilyIndex() == INVALID_UINT32_VALUE) {
            present_queue_ = DevQueue{present_queue_family_index};
        } else if (present_queue_.getFamilyIndex() != present_queue_family_index) {
            logError(LOG_VK "inconsistent queue families for surfaces");
            return false;
        }
    }

    add_queue_create_info(present_queue_.getFamilyIndex(), priority);

    if (caps.needs_compute) {
        compute_queue_ = DevQueue{physical_device_.findSuitableQueueFamily(VK_QUEUE_COMPUTE_BIT)};
        if (compute_queue_.getFamilyIndex() == INVALID_UINT32_VALUE) {
            logError(LOG_VK "couldn't obtain compute queue family index");
            return false;
        }

        add_queue_create_info(compute_queue_.getFamilyIndex(), priority);
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

    graphics_queue_.loadQueueHandle(*this);
    present_queue_.loadQueueHandle(*this);
    compute_queue_.loadQueueHandle(*this);

    return true;
}

bool Device::waitDevice() {
    VkResult result = vkDeviceWaitIdle(device_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "waiting for device failed");
        return false;
    }
    return true;
}

void Device::finalize() {
    if (device_ != VK_NULL_HANDLE) { waitDevice(); }
}

//@{ IDevice

ISwapChain* Device::createSwapChain(ISurface& surface, const SwapChainCreateInfo& create_info) {
    return static_cast<Surface&>(surface).createSwapChain(*this, create_info);
}

bool Device::prepareTestScene(ISurface& surface) {
    // Drawing synchronization

    if (!createSemaphore(sem_image_acquired_)) { return false; }
    if (!createSemaphore(sem_ready_to_present_)) { return false; }
    if (!createFence(true, fence_drawing_)) { return false; }

    // Command buffers creation

    if (!createCommandPool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, graphics_queue_.getFamilyIndex(),
                           command_pool_)) {
        return false;
    }

    // Command buffers creation

    command_buffers_.resize(1, VK_NULL_HANDLE);
    if (!allocateCommandBuffers(command_pool_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, command_buffers_)) { return false; }

    command_buffer_ = CommandBuffer{command_buffers_[0]};

    // Render pass

    auto& surface_impl = static_cast<Surface&>(surface);

    if (!createRenderPass(
            std::array{
                VkAttachmentDescription{
                    .format = surface_impl.getImageFormat().format,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                },
            },
            std::array{
                Wrapper<VkSubpassDescription>::unwrap({
                    .pipeline_type = VK_PIPELINE_BIND_POINT_GRAPHICS,
                    .color_attachments =
                        std::array{
                            VkAttachmentReference{
                                .attachment = 0,
                                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            },
                        },
                }),
            },
            std::array{
                VkSubpassDependency{
                    .srcSubpass = VK_SUBPASS_EXTERNAL,
                    .dstSubpass = 0,
                    .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                },
                VkSubpassDependency{
                    .srcSubpass = 0,
                    .dstSubpass = VK_SUBPASS_EXTERNAL,
                    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                },
            },
            render_pass_)) {
        return false;
    }

    return true;
}

bool Device::renderTestScene(ISwapChain& swap_chain) {
    if (!waitForFences(std::array{fence_drawing_}, false, 5000000000)) { return false; }

    if (!resetFences(std::array{fence_drawing_})) { return false; }

    auto& swap_chain_impl = static_cast<SwapChain&>(swap_chain);

    std::uint32_t image_index = 0;
    if (!swap_chain_impl.acquireImage(2000000000, sem_image_acquired_, VK_NULL_HANDLE, image_index)) { return false; }

    ObjectDestroyer<VkFramebuffer> framebuffer(device_);
    if (!createFramebuffer(render_pass_, std::array{swap_chain_impl.getImageView(image_index)},
                           swap_chain_impl.getImageSize(), 1, ~framebuffer)) {
        return false;
    }

    if (!command_buffer_.beginCommandBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr)) { return false; }

    if (present_queue_.getFamilyIndex() != graphics_queue_.getFamilyIndex()) {
        command_buffer_.setImageMemoryBarrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                              std::array{
                                                  Wrapper<VkImageMemoryBarrier>::unwrap({
                                                      .image = swap_chain_impl.getImage(image_index),
                                                      .current_access = VK_ACCESS_MEMORY_READ_BIT,
                                                      .new_access = VK_ACCESS_MEMORY_READ_BIT,
                                                      .current_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                      .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                      .current_queue_family = present_queue_.getFamilyIndex(),
                                                      .new_queue_family = graphics_queue_.getFamilyIndex(),
                                                      .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                                                  }),
                                              });
    }

    // Drawing

    command_buffer_.beginRenderPass(render_pass_, ~framebuffer, swap_chain_impl.getImageRect(),
                                    std::array{VkClearValue{.color = {{0.2f, 0.5f, 0.8f, 1.0f}}}},
                                    VK_SUBPASS_CONTENTS_INLINE);

    command_buffer_.endRenderPass();

    if (present_queue_.getFamilyIndex() != graphics_queue_.getFamilyIndex()) {
        command_buffer_.setImageMemoryBarrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                              VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                              std::array{
                                                  Wrapper<VkImageMemoryBarrier>::unwrap({
                                                      .image = swap_chain_impl.getImage(image_index),
                                                      .current_access = VK_ACCESS_MEMORY_READ_BIT,
                                                      .new_access = VK_ACCESS_MEMORY_READ_BIT,
                                                      .current_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                      .new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                      .current_queue_family = graphics_queue_.getFamilyIndex(),
                                                      .new_queue_family = present_queue_.getFamilyIndex(),
                                                      .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                                                  }),
                                              });
    }

    if (!command_buffer_.endCommandBuffer()) { return false; }

    if (!present_queue_.submitCommandBuffers(
            {std::array{sem_image_acquired_}, std::array{VkPipelineStageFlags(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)}},
            std::array{~command_buffer_}, std::array{sem_ready_to_present_}, fence_drawing_)) {
        return false;
    }

    if (!present_queue_.presentImages(std::array{sem_ready_to_present_},
                                      {std::array{~swap_chain_impl}, std::array{image_index}})) {
        return false;
    }

    return true;
}

//@}

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
    if (fences.empty()) { return true; }

    VkResult result = vkWaitForFences(device_, std::uint32_t(fences.size()), fences.data(), wait_for_all, timeout);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "waiting for fence failed");
        return false;
    }

    return true;
}

bool Device::resetFences(std::span<const VkFence> fences) {
    if (fences.empty()) { return true; }

    VkResult result = vkResetFences(device_, std::uint32_t(fences.size()), fences.data());
    if (result != VK_SUCCESS) {
        logError(LOG_VK "error occurred when tried to reset fences");
        return false;
    }

    return true;
}

bool Device::createRenderPass(std::span<const VkAttachmentDescription> attachments_descriptions,
                              std::span<const VkSubpassDescription> subpass_descriptions,
                              std::span<const VkSubpassDependency> subpass_dependencies, VkRenderPass& render_pass) {
    const VkRenderPassCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = std::uint32_t(attachments_descriptions.size()),
        .pAttachments = attachments_descriptions.data(),
        .subpassCount = std::uint32_t(subpass_descriptions.size()),
        .pSubpasses = subpass_descriptions.data(),
        .dependencyCount = std::uint32_t(subpass_dependencies.size()),
        .pDependencies = subpass_dependencies.data(),
    };

    VkResult result = vkCreateRenderPass(device_, &create_info, nullptr, &render_pass);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create a render pass");
        return false;
    }

    return true;
}

bool Device::createFramebuffer(VkRenderPass render_pass, std::span<const VkImageView> attachments, VkExtent2D size,
                               std::uint32_t layers, VkFramebuffer& framebuffer) {
    const VkFramebufferCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = render_pass,
        .attachmentCount = std::uint32_t(attachments.size()),
        .pAttachments = attachments.data(),
        .width = size.width,
        .height = size.height,
        .layers = layers,
    };

    VkResult result = vkCreateFramebuffer(device_, &create_info, nullptr, &framebuffer);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create a framebuffer");
        return false;
    }

    return true;
}

bool Device::createCommandPool(VkCommandPoolCreateFlags flags, std::uint32_t queue_family, VkCommandPool& command_pool) {
    const VkCommandPoolCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = flags,
        .queueFamilyIndex = queue_family,
    };

    VkResult result = vkCreateCommandPool(device_, &create_info, nullptr, &command_pool);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create command pool");
        return false;
    }

    return true;
}

bool Device::allocateCommandBuffers(VkCommandPool command_pool, VkCommandBufferLevel level,
                                    std::span<VkCommandBuffer> command_buffers) {
    const VkCommandBufferAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = level,
        .commandBufferCount = std::uint32_t(command_buffers.size()),
    };

    VkResult result = vkAllocateCommandBuffers(device_, &allocate_info, command_buffers.data());
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't allocate command buffers");
        return false;
    }

    return true;
}

// --------------------------------------------------------
// CommandBuffer class implementation

bool CommandBuffer::beginCommandBuffer(VkCommandBufferUsageFlags usage,
                                       VkCommandBufferInheritanceInfo* secondary_command_buffer_info) {
    const VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = usage,
        .pInheritanceInfo = secondary_command_buffer_info,
    };

    VkResult result = vkBeginCommandBuffer(command_buffer_, &begin_info);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't begin command buffer recording operation");
        return false;
    }

    return true;
}

bool CommandBuffer::endCommandBuffer() {
    VkResult result = vkEndCommandBuffer(command_buffer_);
    if (VK_SUCCESS != result) {
        logError(LOG_VK "error occurred during command buffer recording");
        return false;
    }
    return true;
}

void CommandBuffer::setImageMemoryBarrier(VkPipelineStageFlags generating_stages, VkPipelineStageFlags consuming_stages,
                                          std::span<const VkImageMemoryBarrier> image_memory_barriers) {
    if (image_memory_barriers.empty()) { return; }
    vkCmdPipelineBarrier(command_buffer_, generating_stages, consuming_stages, 0, 0, nullptr, 0, nullptr,
                         std::uint32_t(image_memory_barriers.size()), image_memory_barriers.data());
}

void CommandBuffer::beginRenderPass(VkRenderPass render_pass, VkFramebuffer framebuffer, VkRect2D render_area,
                                    std::span<const VkClearValue> clear_values, VkSubpassContents subpass_contents) {
    const VkRenderPassBeginInfo pass_begin_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = render_pass,
        .framebuffer = framebuffer,
        .renderArea = render_area,
        .clearValueCount = std::uint32_t(clear_values.size()),
        .pClearValues = clear_values.data(),
    };

    vkCmdBeginRenderPass(command_buffer_, &pass_begin_info, subpass_contents);
}
