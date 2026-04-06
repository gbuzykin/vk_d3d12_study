#include "pipeline.h"

#include "device.h"
#include "enum_tables.h"
#include "object_destroyer.h"
#include "pipeline_layout.h"
#include "render_target.h"
#include "shader_module.h"
#include "vulkan_logger.h"

#include <uxs/string_cvt.h>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Pipeline class implementation

Pipeline::Pipeline(Device& device, RenderTarget& render_target, PipelineLayout& pipeline_layout)
    : device_(util::not_null{&device}), render_target_(util::not_null{&render_target}),
      pipeline_layout_(util::not_null{&pipeline_layout}) {}

Pipeline::~Pipeline() { ObjectDestroyer<VkPipeline>::destroy(~*device_, pipeline_); }

bool Pipeline::create(std::span<IShaderModule* const> shader_modules, const uxs::db::value& config) {
    // Shader stages :

    uxs::inline_dynarray<VkPipelineShaderStageCreateInfo> shader_stage_create_infos;

    const auto& shader_stages = config.value("stages");
    shader_stage_create_infos.reserve(shader_stages.size());

    for (const auto& module : shader_stages.as_array()) {
        const std::uint32_t index = module.value<std::uint32_t>("module_index");
        if (index >= shader_modules.size()) { throw uxs::db::database_error("shader module index out of range"); }
        shader_stage_create_infos.push_back(VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = parseShaderStage(module.value("stage").as_string_view()),
            .module = ~static_cast<ShaderModule&>(*shader_modules[index]),
            .pName = module.value("entry").as_c_string(),
        });
    }

    // Vertex layouts :

    uxs::inline_dynarray<VkVertexInputBindingDescription> vertex_input_binding_descriptions;
    uxs::inline_dynarray<VkVertexInputAttributeDescription> vertex_attribute_descriptions;

    const auto& vertex_layouts = config.value("vertex_layouts");
    vertex_input_binding_descriptions.reserve(vertex_layouts.size());

    std::uint32_t slot = 0;
    for (const auto& layout : vertex_layouts.as_array()) {
        const std::uint32_t binding = layout.value<std::uint32_t>("binding");
        setBinding(BindingType::VERTEX_BUFFER, slot++, binding);

        vertex_input_binding_descriptions.push_back(VkVertexInputBindingDescription{
            .binding = binding,
            .stride = layout.value<std::uint32_t>("stride"),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        });

        const auto& attributes = layout.value("attributes");
        vertex_attribute_descriptions.reserve(vertex_attribute_descriptions.size() + attributes.size());

        for (const auto& [location, attribute] : attributes.as_record()) {
            vertex_attribute_descriptions.push_back(VkVertexInputAttributeDescription{
                .location = uxs::from_string<std::uint32_t>(location),
                .binding = binding,
                .format = parseInputFormat(attribute.value("format").as_string_view()),
                .offset = attribute.value<std::uint32_t>("offset"),
            });
        }
    }

    // Others :

    const VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = std::uint32_t(vertex_input_binding_descriptions.size()),
        .pVertexBindingDescriptions = vertex_input_binding_descriptions.data(),
        .vertexAttributeDescriptionCount = std::uint32_t(vertex_attribute_descriptions.size()),
        .pVertexAttributeDescriptions = vertex_attribute_descriptions.data(),
    };

    const VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    const std::array<VkViewport, 1> viewports{};
    const std::array<VkRect2D, 1> scissors{};
    const VkPipelineViewportStateCreateInfo viewport_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = std::uint32_t(viewports.size()),
        .pViewports = viewports.data(),
        .scissorCount = std::uint32_t(scissors.size()),
        .pScissors = scissors.data(),
    };

    const VkPipelineRasterizationStateCreateInfo rasterization_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f,
    };

    const VkPipelineMultisampleStateCreateInfo multisample_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0.0f,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    const VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    const std::array attachment_blend_states{
        VkPipelineColorBlendAttachmentState{
            .blendEnable = VK_FALSE,
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
    const VkPipelineColorBlendStateCreateInfo blend_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = std::uint32_t(attachment_blend_states.size()),
        .pAttachments = attachment_blend_states.data(),
        .blendConstants = {1.0f, 1.0f, 1.0f, 1.0f},
    };

    const std::array dynamic_states{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
    };
    const VkPipelineDynamicStateCreateInfo dynamic_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = std::uint32_t(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };

    const VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = std::uint32_t(shader_stage_create_infos.size()),
        .pStages = shader_stage_create_infos.data(),
        .pVertexInputState = &vertex_input_state_create_info,
        .pInputAssemblyState = &input_assembly_state_create_info,
        .pViewportState = &viewport_state_create_info,
        .pRasterizationState = &rasterization_state_create_info,
        .pMultisampleState = &multisample_state_create_info,
        .pDepthStencilState = render_target_->useDepth() ? &depth_stencil_state_create_info : nullptr,
        .pColorBlendState = &blend_state_create_info,
        .pDynamicState = &dynamic_state_create_info,
        .layout = ~*pipeline_layout_,
        .renderPass = render_target_->getRenderPass(),
        .basePipelineIndex = -1,
    };

    VkResult result = vkCreateGraphicsPipelines(~*device_, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr,
                                                &pipeline_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create graphics pipeline: {}", result);
        return false;
    }

    return true;
}

//@{ IPipeline

//@}
