#pragma once

#include "vulkan_api.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <span>

namespace app3d::rel::vulkan {

template<typename Ty>
struct Wrapper;

template<>
struct Wrapper<VkImageMemoryBarrier> {
    VkImage image;
    VkAccessFlags current_access;
    VkAccessFlags new_access;
    VkImageLayout current_layout;
    VkImageLayout new_layout;
    std::uint32_t current_queue_family;
    std::uint32_t new_queue_family;
    VkImageAspectFlags aspect;
    static VkImageMemoryBarrier unwrap(Wrapper wrapper) {
        return {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = wrapper.current_access,
            .dstAccessMask = wrapper.new_access,
            .oldLayout = wrapper.current_layout,
            .newLayout = wrapper.new_layout,
            .srcQueueFamilyIndex = wrapper.current_queue_family,
            .dstQueueFamilyIndex = wrapper.new_queue_family,
            .image = wrapper.image,
            .subresourceRange =
                {
                    .aspectMask = wrapper.aspect,
                    .baseMipLevel = 0,
                    .levelCount = VK_REMAINING_MIP_LEVELS,
                    .baseArrayLayer = 0,
                    .layerCount = VK_REMAINING_ARRAY_LAYERS,
                },
        };
    }
};

template<>
struct Wrapper<VkBufferMemoryBarrier> {
    VkBuffer buffer;
    VkAccessFlags current_access;
    VkAccessFlags new_access;
    std::uint32_t current_queue_family;
    std::uint32_t new_queue_family;
    static VkBufferMemoryBarrier unwrap(Wrapper wrapper) {
        return {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = wrapper.current_access,
            .dstAccessMask = wrapper.new_access,
            .srcQueueFamilyIndex = wrapper.current_queue_family,
            .dstQueueFamilyIndex = wrapper.new_queue_family,
            .buffer = wrapper.buffer,
            .offset = 0,
            .size = VK_WHOLE_SIZE,
        };
    }
};

template<>
struct Wrapper<VkSubpassDescription> {
    VkPipelineBindPoint pipeline_type;
    std::span<const VkAttachmentReference> input_attachments;
    std::span<const VkAttachmentReference> color_attachments;
    std::span<const VkAttachmentReference> resolve_attachments;
    const VkAttachmentReference* depth_stencil_attachment;
    std::span<const std::uint32_t> preserve_attachments;
    static VkSubpassDescription unwrap(Wrapper wrapper) {
        assert(wrapper.resolve_attachments.empty() ||
               wrapper.resolve_attachments.size() == wrapper.color_attachments.size());
        return {
            .pipelineBindPoint = wrapper.pipeline_type,
            .inputAttachmentCount = std::uint32_t(wrapper.input_attachments.size()),
            .pInputAttachments = wrapper.input_attachments.data(),
            .colorAttachmentCount = std::uint32_t(wrapper.color_attachments.size()),
            .pColorAttachments = wrapper.color_attachments.data(),
            .pResolveAttachments = !wrapper.resolve_attachments.empty() ? wrapper.resolve_attachments.data() : nullptr,
            .pDepthStencilAttachment = wrapper.depth_stencil_attachment,
            .preserveAttachmentCount = std::uint32_t(wrapper.preserve_attachments.size()),
            .pPreserveAttachments = wrapper.preserve_attachments.data(),
        };
    }
};

template<>
struct Wrapper<VkPipelineShaderStageCreateInfo> {
    VkShaderStageFlagBits shader_stage;
    VkShaderModule shader_module;
    const char* entry_point_name;
    const VkSpecializationInfo* specialization_info;
    static VkPipelineShaderStageCreateInfo unwrap(Wrapper wrapper) {
        return {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = wrapper.shader_stage,
            .module = wrapper.shader_module,
            .pName = wrapper.entry_point_name,
            .pSpecializationInfo = wrapper.specialization_info,
        };
    }
};

template<>
struct Wrapper<VkPipelineVertexInputStateCreateInfo> {
    std::span<const VkVertexInputBindingDescription> binding_descriptions;
    std::span<const VkVertexInputAttributeDescription> attribute_descriptions;
    static VkPipelineVertexInputStateCreateInfo unwrap(Wrapper wrapper) {
        return {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = std::uint32_t(wrapper.binding_descriptions.size()),
            .pVertexBindingDescriptions = wrapper.binding_descriptions.data(),
            .vertexAttributeDescriptionCount = std::uint32_t(wrapper.attribute_descriptions.size()),
            .pVertexAttributeDescriptions = wrapper.attribute_descriptions.data(),
        };
    }
};

template<>
struct Wrapper<VkPipelineInputAssemblyStateCreateInfo> {
    VkPrimitiveTopology topology;
    bool primitive_restart_enable;
    static VkPipelineInputAssemblyStateCreateInfo unwrap(Wrapper wrapper) {
        return {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = wrapper.topology,
            .primitiveRestartEnable = wrapper.primitive_restart_enable,
        };
    }
};

template<>
struct Wrapper<VkPipelineViewportStateCreateInfo> {
    std::span<const VkViewport> viewports;
    std::span<const VkRect2D> scissors;
    static VkPipelineViewportStateCreateInfo unwrap(Wrapper wrapper) {
        return {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = std::uint32_t(wrapper.viewports.size()),
            .pViewports = wrapper.viewports.data(),
            .scissorCount = std::uint32_t(wrapper.scissors.size()),
            .pScissors = wrapper.scissors.data(),
        };
    }
};

template<>
struct Wrapper<VkPipelineRasterizationStateCreateInfo> {
    bool depth_clamp_enable;
    bool rasterizer_discard_enable;
    VkPolygonMode polygon_mode;
    VkCullModeFlags culling_mode;
    VkFrontFace front_face;
    bool depth_bias_enable;
    float depth_bias_constant_factor;
    float depth_bias_clamp;
    float depth_bias_slope_factor;
    float line_width;
    static VkPipelineRasterizationStateCreateInfo unwrap(Wrapper wrapper) {
        return {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = wrapper.depth_clamp_enable,
            .rasterizerDiscardEnable = wrapper.rasterizer_discard_enable,
            .polygonMode = wrapper.polygon_mode,
            .cullMode = wrapper.culling_mode,
            .frontFace = wrapper.front_face,
            .depthBiasEnable = wrapper.depth_bias_enable,
            .depthBiasConstantFactor = wrapper.depth_bias_constant_factor,
            .depthBiasClamp = wrapper.depth_bias_clamp,
            .depthBiasSlopeFactor = wrapper.depth_bias_slope_factor,
            .lineWidth = wrapper.line_width,
        };
    }
};

template<>
struct Wrapper<VkPipelineMultisampleStateCreateInfo> {
    VkSampleCountFlagBits sample_count;
    bool per_sample_shading_enable;
    float min_sample_shading;
    const VkSampleMask* sample_masks;
    bool alpha_to_coverage_enable;
    bool alpha_to_one_enable;
    static VkPipelineMultisampleStateCreateInfo unwrap(Wrapper wrapper) {
        return {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = wrapper.sample_count,
            .sampleShadingEnable = wrapper.per_sample_shading_enable,
            .minSampleShading = wrapper.min_sample_shading,
            .pSampleMask = wrapper.sample_masks,
            .alphaToCoverageEnable = wrapper.alpha_to_coverage_enable,
            .alphaToOneEnable = wrapper.alpha_to_one_enable,
        };
    }
};

template<>
struct Wrapper<VkPipelineColorBlendStateCreateInfo> {
    bool logic_op_enable;
    VkLogicOp logic_op;
    std::span<const VkPipelineColorBlendAttachmentState> attachment_blend_states;
    std::array<float, 4> blend_constants;
    static VkPipelineColorBlendStateCreateInfo unwrap(Wrapper wrapper) {
        return {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = wrapper.logic_op_enable,
            .logicOp = wrapper.logic_op,
            .attachmentCount = std::uint32_t(wrapper.attachment_blend_states.size()),
            .pAttachments = wrapper.attachment_blend_states.data(),
            .blendConstants = {wrapper.blend_constants[0], wrapper.blend_constants[1], wrapper.blend_constants[2],
                               wrapper.blend_constants[3]},
        };
    }
};

template<>
struct Wrapper<VkPipelineDynamicStateCreateInfo> {
    std::span<const VkDynamicState> dynamic_states;
    static VkPipelineDynamicStateCreateInfo unwrap(Wrapper wrapper) {
        return {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = std::uint32_t(wrapper.dynamic_states.size()),
            .pDynamicStates = wrapper.dynamic_states.data(),
        };
    }
};

template<>
struct Wrapper<VkGraphicsPipelineCreateInfo> {
    VkPipelineCreateFlags additional_options;
    std::span<const VkPipelineShaderStageCreateInfo> shader_stage_create_infos;
    const VkPipelineVertexInputStateCreateInfo* vertex_input_state_create_info;
    const VkPipelineInputAssemblyStateCreateInfo* input_assembly_state_create_info;
    const VkPipelineTessellationStateCreateInfo* tessellation_state_create_info;
    const VkPipelineViewportStateCreateInfo* viewport_state_create_info;
    const VkPipelineRasterizationStateCreateInfo* rasterization_state_create_info;
    const VkPipelineMultisampleStateCreateInfo* multisample_state_create_info;
    const VkPipelineDepthStencilStateCreateInfo* depth_and_stencil_state_create_info;
    const VkPipelineColorBlendStateCreateInfo* blend_state_create_info;
    const VkPipelineDynamicStateCreateInfo* dynamic_state_creat_info;
    VkPipelineLayout pipeline_layout;
    VkRenderPass render_pass;
    std::uint32_t subpass;
    VkPipeline base_pipeline_handle;
    std::int32_t base_pipeline_index;
    static VkGraphicsPipelineCreateInfo unwrap(Wrapper wrapper) {
        return {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .flags = wrapper.additional_options,
            .stageCount = std::uint32_t(wrapper.shader_stage_create_infos.size()),
            .pStages = wrapper.shader_stage_create_infos.data(),
            .pVertexInputState = wrapper.vertex_input_state_create_info,
            .pInputAssemblyState = wrapper.input_assembly_state_create_info,
            .pTessellationState = wrapper.tessellation_state_create_info,
            .pViewportState = wrapper.viewport_state_create_info,
            .pRasterizationState = wrapper.rasterization_state_create_info,
            .pMultisampleState = wrapper.multisample_state_create_info,
            .pDepthStencilState = wrapper.depth_and_stencil_state_create_info,
            .pColorBlendState = wrapper.blend_state_create_info,
            .pDynamicState = wrapper.dynamic_state_creat_info,
            .layout = wrapper.pipeline_layout,
            .renderPass = wrapper.render_pass,
            .subpass = wrapper.subpass,
            .basePipelineHandle = wrapper.base_pipeline_handle,
            .basePipelineIndex = wrapper.base_pipeline_index,
        };
    }
};

}  // namespace app3d::rel::vulkan
