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
    ObjectDestroyer<VkBuffer>::destroy(device_, vertex_buffer_);
    for (const auto& pipeline : graphics_pipelines_) { ObjectDestroyer<VkPipeline>::destroy(device_, pipeline); }
    ObjectDestroyer<VkPipelineLayout>::destroy(device_, pipeline_layout_);
    ObjectDestroyer<VkShaderModule>::destroy(device_, vertex_shader_module_);
    ObjectDestroyer<VkShaderModule>::destroy(device_, fragment_shader_module_);
    ObjectDestroyer<VkRenderPass>::destroy(device_, render_pass_);
    ObjectDestroyer<VkCommandPool>::destroy(device_, command_pool_);
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

    // Graphics pipeline

    std::vector<std::uint8_t> vertex_shader_spirv;
    std::vector<std::uint8_t> fragment_shader_spirv;

    if (!createShaderModule(vertex_shader_spirv, vertex_shader_module_)) { return false; }
    if (!createShaderModule(fragment_shader_spirv, fragment_shader_module_)) { return false; }

    const std::array shader_stage_create_infos{
        Wrapper<VkPipelineShaderStageCreateInfo>::unwrap({
            .shader_stage = VK_SHADER_STAGE_VERTEX_BIT,
            .shader_module = vertex_shader_module_,
            .entry_point_name = "main",
        }),
        Wrapper<VkPipelineShaderStageCreateInfo>::unwrap({
            .shader_stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .shader_module = fragment_shader_module_,
            .entry_point_name = "main",
        }),
    };

    const std::array vertex_input_binding_descriptions{
        VkVertexInputBindingDescription{
            .binding = 0,
            .stride = 3 * sizeof(float),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
    };
    const std::array vertex_attribute_descriptions{
        VkVertexInputAttributeDescription{
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0,
        },
    };
    const auto vertex_input_state_create_info = Wrapper<VkPipelineVertexInputStateCreateInfo>::unwrap({
        .binding_descriptions = vertex_input_binding_descriptions,
        .attribute_descriptions = vertex_attribute_descriptions,
    });

    const auto input_assembly_state_create_info = Wrapper<VkPipelineInputAssemblyStateCreateInfo>::unwrap({
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        .primitive_restart_enable = false,
    });

    const std::array<VkViewport, 1> viewports{};
    const std::array<VkRect2D, 1> scissors{};
    const auto viewport_state_create_info = Wrapper<VkPipelineViewportStateCreateInfo>::unwrap({
        .viewports = viewports,
        .scissors = scissors,
    });

    const auto rasterization_state_create_info = Wrapper<VkPipelineRasterizationStateCreateInfo>::unwrap({
        .depth_clamp_enable = false,
        .rasterizer_discard_enable = false,
        .polygon_mode = VK_POLYGON_MODE_FILL,
        .culling_mode = VK_CULL_MODE_BACK_BIT,
        .front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depth_bias_enable = false,
        .depth_bias_constant_factor = 0.0f,
        .depth_bias_clamp = 0.0f,
        .depth_bias_slope_factor = 0.0f,
        .line_width = 1.0f,
    });

    const auto multisample_state_create_info = Wrapper<VkPipelineMultisampleStateCreateInfo>::unwrap({
        .sample_count = VK_SAMPLE_COUNT_1_BIT,
        .per_sample_shading_enable = false,
        .min_sample_shading = 0.0f,
        .alpha_to_coverage_enable = false,
        .alpha_to_one_enable = false,
    });

    const std::array attachment_blend_states{
        VkPipelineColorBlendAttachmentState{
            .blendEnable = false,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                              VK_COLOR_COMPONENT_A_BIT,
        },
    };
    const auto blend_state_create_info = Wrapper<VkPipelineColorBlendStateCreateInfo>::unwrap({
        .logic_op_enable = false,
        .logic_op = VK_LOGIC_OP_COPY,
        .attachment_blend_states = attachment_blend_states,
        .blend_constants = {1.0f, 1.0f, 1.0f, 1.0f},
    });

    const std::array dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const auto dynamic_state_create_info = Wrapper<VkPipelineDynamicStateCreateInfo>::unwrap({
        .dynamic_states = dynamic_states,
    });

    if (!createPipelineLayout({}, {}, pipeline_layout_)) { return false; }

    const std::array graphics_pipeline_create_infos{
        Wrapper<VkGraphicsPipelineCreateInfo>::unwrap({
            .shader_stage_create_infos = shader_stage_create_infos,
            .vertex_input_state_create_info = &vertex_input_state_create_info,
            .input_assembly_state_create_info = &input_assembly_state_create_info,
            .viewport_state_create_info = &viewport_state_create_info,
            .rasterization_state_create_info = &rasterization_state_create_info,
            .multisample_state_create_info = &multisample_state_create_info,
            .blend_state_create_info = &blend_state_create_info,
            .dynamic_state_creat_info = &dynamic_state_create_info,
            .pipeline_layout = pipeline_layout_,
            .render_pass = render_pass_,
            .base_pipeline_index = -1,
        }),
    };

    graphics_pipelines_.resize(1, VK_NULL_HANDLE);
    if (!createGraphicsPipelines({graphics_pipeline_create_infos, graphics_pipelines_}, VK_NULL_HANDLE)) {
        return false;
    }

    graphics_pipeline_ = graphics_pipelines_[0];

    // Vertex data
    std::vector<float> vertices{0.0f, -0.75f, 0.0f, -0.75f, 0.75f, 0.0f, 0.75f, 0.75f, 0.0f};

    if (!createBuffer(sizeof(vertices[0]) * vertices.size(),
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertex_buffer_)) {
        return false;
    }

#if 0
    InitVkDestroyer( LogicalDevice, BufferMemory );
    if( !AllocateAndBindMemoryObjectToBuffer( PhysicalDevice, *LogicalDevice, *VertexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, *BufferMemory ) ) {
      return false;
    }

    if( !UseStagingBufferToUpdateBufferWithDeviceLocalMemoryBound( PhysicalDevice, *LogicalDevice, sizeof( vertices[0] ) * vertices.size(), &vertices[0], *VertexBuffer, 0, 0,
      VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, GraphicsQueue.Handle, CommandBuffer, {} ) ) {
      return false;
    }
#endif

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
                                    std::array{VkClearValue{.color = {{0.1f, 0.2f, 0.3f, 1.0f}}}},
                                    VK_SUBPASS_CONTENTS_INLINE);

#if 0
    BindPipelineObject( CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *GraphicsPipeline );
    VkViewport viewport = {
      0.0f,                                       // float    x
      0.0f,                                       // float    y
      static_cast<float>(Swapchain.Size.width),   // float    width
      static_cast<float>(Swapchain.Size.height),  // float    height
      0.0f,                                       // float    minDepth
      1.0f,                                       // float    maxDepth
    };
    SetViewportStateDynamically( CommandBuffer, 0, { viewport } );

    VkRect2D scissor = {
      {                                         // VkOffset2D     offset
        0,                                        // int32_t        x
        0                                         // int32_t        y
      },
      {                                         // VkExtent2D     extent
        Swapchain.Size.width,                     // uint32_t       width
        Swapchain.Size.height                     // uint32_t       height
      }
    };
    SetScissorStateDynamically( CommandBuffer, 0, { scissor } );

    BindVertexBuffers( CommandBuffer, 0, { { *VertexBuffer, 0 } } );

    DrawGeometry( CommandBuffer, 3, 1, 0, 0 );
#endif

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

bool Device::createShaderModule(std::span<std::uint8_t> source_code, VkShaderModule& shader_module) {
    const VkShaderModuleCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = source_code.size(),
        .pCode = reinterpret_cast<const std::uint32_t*>(source_code.data()),
    };

    VkResult result = vkCreateShaderModule(device_, &create_info, nullptr, &shader_module);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create a shader module");
        return false;
    }

    return true;
}

bool Device::createPipelineLayout(std::span<const VkDescriptorSetLayout> descriptor_set_layouts,
                                  std::span<const VkPushConstantRange> push_constant_ranges,
                                  VkPipelineLayout& pipeline_layout) {
    const VkPipelineLayoutCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = std::uint32_t(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = std::uint32_t(push_constant_ranges.size()),
        .pPushConstantRanges = push_constant_ranges.data(),
    };

    VkResult result = vkCreatePipelineLayout(device_, &create_info, nullptr, &pipeline_layout);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create pipeline layout");
        return false;
    }

    return true;
}

bool Device::createGraphicsPipelines(MultiSpan<const VkGraphicsPipelineCreateInfo, VkPipeline> pipelines,
                                     VkPipelineCache pipeline_cache) {
    if (pipelines.empty()) { return false; }
    VkResult result = vkCreateGraphicsPipelines(device_, pipeline_cache, std::uint32_t(pipelines.size()),
                                                pipelines.data<0>(), nullptr, pipelines.data<1>());
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create a graphics pipeline");
        return false;
    }
    return false;
}

bool Device::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer) {
    const VkBufferCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkResult result = vkCreateBuffer(device_, &create_info, nullptr, &buffer);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create a buffer");
        return false;
    }

    return true;
}

#if 0
bool AllocateAndBindMemoryObjectToBuffer(VkPhysicalDevice physical_device, VkDevice logical_device, VkBuffer buffer,
                                         VkMemoryPropertyFlagBits memory_properties, VkDeviceMemory& memory_object) {
    VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &physical_device_memory_properties);

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(logical_device, buffer, &memory_requirements);

    memory_object = VK_NULL_HANDLE;
    for (uint32_t type = 0; type < physical_device_memory_properties.memoryTypeCount; ++type) {
        if ((memory_requirements.memoryTypeBits & (1 << type)) &&
            ((physical_device_memory_properties.memoryTypes[type].propertyFlags & memory_properties) ==
             memory_properties)) {
            VkMemoryAllocateInfo buffer_memory_allocate_info = {
                VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,  // VkStructureType    sType
                nullptr,                                 // const void       * pNext
                memory_requirements.size,                // VkDeviceSize       allocationSize
                type                                     // uint32_t           memoryTypeIndex
            };

            VkResult result = vkAllocateMemory(logical_device, &buffer_memory_allocate_info, nullptr, &memory_object);
            if (VK_SUCCESS == result) { break; }
        }
    }

    if (VK_NULL_HANDLE == memory_object) {
        std::cout << "Could not allocate memory for a buffer." << std::endl;
        return false;
    }

    VkResult result = vkBindBufferMemory(logical_device, buffer, memory_object, 0);
    if (VK_SUCCESS != result) {
        std::cout << "Could not bind memory object to a buffer." << std::endl;
        return false;
    }
    return true;
}

bool UseStagingBufferToUpdateBufferWithDeviceLocalMemoryBound(
    VkPhysicalDevice physical_device, VkDevice logical_device, VkDeviceSize data_size, void* data,
    VkBuffer destination_buffer, VkDeviceSize destination_offset, VkAccessFlags destination_buffer_current_access,
    VkAccessFlags destination_buffer_new_access, VkPipelineStageFlags destination_buffer_generating_stages,
    VkPipelineStageFlags destination_buffer_consuming_stages, VkQueue queue, VkCommandBuffer command_buffer,
    std::vector<VkSemaphore> signal_semaphores) {
    VkDestroyer(VkBuffer) staging_buffer;
    InitVkDestroyer(logical_device, staging_buffer);
    if (!CreateBuffer(logical_device, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, *staging_buffer)) { return false; }

    VkDestroyer(VkDeviceMemory) memory_object;
    InitVkDestroyer(logical_device, memory_object);
    if (!AllocateAndBindMemoryObjectToBuffer(physical_device, logical_device, *staging_buffer,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, *memory_object)) {
        return false;
    }

    if (!MapUpdateAndUnmapHostVisibleMemory(logical_device, *memory_object, 0, data_size, data, true, nullptr)) {
        return false;
    }

    if (!BeginCommandBufferRecordingOperation(command_buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr)) {
        return false;
    }

    SetBufferMemoryBarrier(command_buffer, destination_buffer_generating_stages, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           {{destination_buffer, destination_buffer_current_access, VK_ACCESS_TRANSFER_WRITE_BIT,
                             VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED}});

    CopyDataBetweenBuffers(command_buffer, *staging_buffer, destination_buffer, {{0, destination_offset, data_size}});

    SetBufferMemoryBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, destination_buffer_consuming_stages,
                           {{destination_buffer, VK_ACCESS_TRANSFER_WRITE_BIT, destination_buffer_new_access,
                             VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED}});

    if (!EndCommandBufferRecordingOperation(command_buffer)) { return false; }

    VkDestroyer(VkFence) fence;
    InitVkDestroyer(logical_device, fence);
    if (!CreateFence(logical_device, false, *fence)) { return false; }

    if (!SubmitCommandBuffersToQueue(queue, {}, {command_buffer}, signal_semaphores, *fence)) { return false; }

    if (!WaitForFences(logical_device, {*fence}, VK_FALSE, 500000000)) { return false; }

    return true;
}
#endif

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
