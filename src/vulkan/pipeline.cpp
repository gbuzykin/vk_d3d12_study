#include "pipeline.h"

#include "device.h"
#include "pipeline_layout.h"
#include "render_target.h"
#include "shader_module.h"
#include "tables.h"
#include "vulkan_logger.h"

#include "rel/tables.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Pipeline class implementation

Pipeline::Pipeline(Device& device, RenderTarget& render_target, PipelineLayout& pipeline_layout)
    : device_(util::not_null{&device}), render_target_(util::not_null{&render_target}),
      pipeline_layout_(util::not_null{&pipeline_layout}) {}

Pipeline::~Pipeline() { device_->vkDestroyPipeline(pipeline_, nullptr); }

bool Pipeline::create(std::span<IShaderModule* const> shader_modules, const uxs::db::value& config) {
    // Shader stages :

    uxs::inline_dynarray<VkPipelineShaderStageCreateInfo> shader_stage_create_infos;

    const auto& shader_stages = config.value("stages");
    shader_stage_create_infos.reserve(shader_stages.size());

    std::uint32_t def_module_index = 0;
    for (const auto& module : shader_stages.as_array()) {
        const std::uint32_t index = module.value_or<std::uint32_t>("module_index", def_module_index++);
        if (index >= shader_modules.size()) { throw uxs::db::database_error("shader module index out of range"); }
        const auto shader_stage = parseShaderStage(module.value("stage").as_string_view());
        shader_stage_create_infos.emplace_back(VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = TBL_VK_SHADER_STAGE[unsigned(shader_stage)],
            .module = static_cast<ShaderModule&>(*shader_modules[index]).getHandle(),
            .pName = module.value_or<const char*>("entry", "main"),
        });
    }

    // Vertex layouts :

    uxs::inline_dynarray<VkVertexInputBindingDescription> vertex_input_binding_descriptions;
    uxs::inline_dynarray<VkVertexInputAttributeDescription> vertex_attribute_descriptions;

    const auto& vertex_layouts = config.value("vertex_layouts");

    std::uint32_t def_slot = 0;

    for (const auto& layout : vertex_layouts.as_array()) {
        const std::uint32_t slot = layout.value_or<std::uint32_t>("slot", def_slot);
        def_slot = slot + 1;

        const auto& attributes = layout.value("attributes");

        const auto align_up = [](auto v, auto alignment) { return (v + alignment - 1) & ~(alignment - 1); };

        std::uint32_t def_location = 0;
        std::uint32_t def_offset = 0;
        std::uint32_t max_alignment = 0;

        for (const auto& attribute : attributes.as_array()) {
            const std::uint32_t location = attribute.value_or<std::uint32_t>("location", def_location);
            def_location = location + 1;

            const auto format = parseFormat(attribute.value("format").as_string_view());
            const std::uint32_t attribute_alignment = TBL_FORMAT_ALIGNMENT[unsigned(format)];
            def_offset = align_up(def_offset, attribute_alignment);
            max_alignment = std::max(attribute_alignment, max_alignment);

            const std::uint32_t offset = attribute.value_or<std::uint32_t>("offset", def_offset);
            def_offset = offset + TBL_FORMAT_SIZE[unsigned(format)];

            vertex_attribute_descriptions.emplace_back(VkVertexInputAttributeDescription{
                .location = location, .binding = slot, .format = TBL_VK_FORMAT[unsigned(format)], .offset = offset});
        }

        const std::uint32_t stride = layout.value_or<std::uint32_t>("stride", align_up(def_offset, max_alignment));
        vertex_input_binding_descriptions.emplace_back(VkVertexInputBindingDescription{
            .binding = slot,
            .stride = stride,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        });
    }

    // Others :

    const VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = std::uint32_t(vertex_input_binding_descriptions.size()),
        .pVertexBindingDescriptions = vertex_input_binding_descriptions.data(),
        .vertexAttributeDescriptionCount = std::uint32_t(vertex_attribute_descriptions.size()),
        .pVertexAttributeDescriptions = vertex_attribute_descriptions.data(),
    };

    const auto topology = parsePrimitiveTopology(config.value_or<std::string_view>("primitive_topology", "TRIANGLES"));

    const VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = TBL_VK_PRIMITIVE_TOPOLOGY[unsigned(topology)],
        .primitiveRestartEnable = VK_FALSE,
    };

    const VkPipelineViewportStateCreateInfo viewport_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
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

    uxs::inline_dynarray<VkDynamicState> dynamic_states;
    dynamic_states.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamic_states.push_back(VK_DYNAMIC_STATE_SCISSOR);
    if (config.value<bool>("dynamic_primitive_topology")) {
        dynamic_states.push_back(VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY);
    }
    if (config.value<bool>("dynamic_vertex_stride")) {
        dynamic_states.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE);
    }

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
        .layout = pipeline_layout_->getHandle(),
        .renderPass = render_target_->getRenderPass(),
        .basePipelineIndex = -1,
    };

    VkResult result = device_->vkCreateGraphicsPipelines(VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr,
                                                         &pipeline_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create graphics pipeline: {}", result);
        return false;
    }

    return true;
}

//@{ IPipeline

//@}
