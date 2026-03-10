#include "pipeline.h"

#include "device.h"
#include "object_destroyer.h"
#include "render_target.h"
#include "wrappers.h"

#include "utils/logger.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Pipeline class implementation

Pipeline::Pipeline(Device& device) : device_(device) {}

Pipeline::~Pipeline() {
    ObjectDestroyer<VkPipeline>::destroy(~device_, pipeline_);
    ObjectDestroyer<VkPipelineLayout>::destroy(~device_, pipeline_layout_);
}

bool Pipeline::create(RenderTarget& render_target,
                      std::span<const VkPipelineShaderStageCreateInfo> shader_stage_create_infos,
                      const uxs::db::value& create_info) {
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

    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
    std::vector<VkPushConstantRange> push_constant_ranges;

    const VkPipelineLayoutCreateInfo pipeline_layout_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = std::uint32_t(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = std::uint32_t(push_constant_ranges.size()),
        .pPushConstantRanges = push_constant_ranges.data(),
    };

    VkResult result = vkCreatePipelineLayout(~device_, &pipeline_layout_create_info, nullptr, &pipeline_layout_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create pipeline layout");
        return false;
    }

    const auto graphics_pipeline_create_info = Wrapper<VkGraphicsPipelineCreateInfo>::unwrap({
        .shader_stage_create_infos = shader_stage_create_infos,
        .vertex_input_state_create_info = &vertex_input_state_create_info,
        .input_assembly_state_create_info = &input_assembly_state_create_info,
        .viewport_state_create_info = &viewport_state_create_info,
        .rasterization_state_create_info = &rasterization_state_create_info,
        .multisample_state_create_info = &multisample_state_create_info,
        .blend_state_create_info = &blend_state_create_info,
        .dynamic_state_creat_info = &dynamic_state_create_info,
        .pipeline_layout = pipeline_layout_,
        .render_pass = render_target.getRenderPassHandle(),
        .base_pipeline_index = -1,
    });

    result = vkCreateGraphicsPipelines(~device_, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &pipeline_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create a graphics pipeline");
        return false;
    }

    return true;
}

//@{ IPipeline

//@}
