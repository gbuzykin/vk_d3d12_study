#include "pipeline.h"

#include "device.h"
#include "object_destroyer.h"
#include "render_target.h"
#include "shader_module.h"
#include "vulkan_logger.h"

#include <array>
#include <unordered_map>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Pipeline class implementation

Pipeline::Pipeline(Device& device) : device_(device) {}

Pipeline::~Pipeline() {
    ObjectDestroyer<VkPipeline>::destroy(~device_, pipeline_);
    ObjectDestroyer<VkPipelineLayout>::destroy(~device_, pipeline_layout_);
    ObjectDestroyer<VkDescriptorSetLayout>::destroy(~device_, descriptor_set_layout_);
}

namespace {
const std::unordered_map<std::string_view, VkShaderStageFlagBits> g_shader_stages{
    {"ALL", VK_SHADER_STAGE_ALL_GRAPHICS},
    {"VERTEX", VK_SHADER_STAGE_VERTEX_BIT},
    {"PIXEL", VK_SHADER_STAGE_FRAGMENT_BIT},
};
const std::unordered_map<std::string_view, VkFormat> g_formats{
    {"FLOAT", VK_FORMAT_R32_SFLOAT},        {"FLOAT2", VK_FORMAT_R32G32_SFLOAT},
    {"FLOAT3", VK_FORMAT_R32G32B32_SFLOAT}, {"FLOAT4", VK_FORMAT_R32G32B32A32_SFLOAT},
    {"BYTE4", VK_FORMAT_R8G8B8A8_UNORM},
};
const std::unordered_map<VkFormat, std::pair<std::uint32_t, std::uint32_t>> g_format_size_alignment{
    {VK_FORMAT_R32_SFLOAT, {4, 4}},           {VK_FORMAT_R32G32_SFLOAT, {8, 4}},  {VK_FORMAT_R32G32B32_SFLOAT, {12, 4}},
    {VK_FORMAT_R32G32B32A32_SFLOAT, {16, 4}}, {VK_FORMAT_R8G8B8A8_UNORM, {4, 4}},
};

VkShaderStageFlagBits parseShaderStage(std::string_view stage) {
    auto it = g_shader_stages.find(stage);
    if (it != g_shader_stages.end()) { return it->second; }
    throw uxs::db::database_error("unknown shader stage");
}

VkFormat parseFormat(std::string_view fmt) {
    auto it = g_formats.find(fmt);
    if (it != g_formats.end()) { return it->second; }
    throw uxs::db::database_error("unknown format");
}

std::pair<std::uint32_t, std::uint32_t> getFormatSizeAlignment(VkFormat fmt) {
    auto it = g_format_size_alignment.find(fmt);
    if (it != g_format_size_alignment.end()) { return it->second; }
    logError(LOG_VK "unknown format");
    return {0, 0};
}
}  // namespace

bool Pipeline::create(RenderTarget& render_target, std::span<IShaderModule* const> shader_modules,
                      const uxs::db::value& config) {
    // Shader stages :

    uxs::inline_dynarray<VkPipelineShaderStageCreateInfo> shader_stage_create_infos;

    const auto& shader_stages = config.value("stages");
    shader_stage_create_infos.reserve(shader_stages.size());

    std::uint32_t def_module_index = 0;
    for (const auto& module : shader_stages.as_array()) {
        const std::uint32_t index = module.value_or<std::uint32_t>("module_index", def_module_index++);
        if (index >= shader_modules.size()) { throw uxs::db::database_error("shader module index out of range"); }
        shader_stage_create_infos.emplace_back(VkPipelineShaderStageCreateInfo{
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
            const auto attribute_size_alignment = getFormatSizeAlignment(format);
            def_offset = align_up(def_offset, attribute_size_alignment.second);
            max_alignment = std::max(attribute_size_alignment.second, max_alignment);

            const std::uint32_t offset = attribute.value_or<std::uint32_t>("offset", def_offset);
            def_offset = offset + attribute_size_alignment.first;

            vertex_attribute_descriptions.emplace_back(VkVertexInputAttributeDescription{
                .location = location, .binding = slot, .format = format, .offset = offset});
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

    const VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
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

    const std::array dynamic_states{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    const VkPipelineDynamicStateCreateInfo dynamic_state_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = std::uint32_t(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };

    if (!createDescriptorSetLayout(std::array{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
        })) {
        return false;
    }

    if (!createPipelineLayout(std::array{descriptor_set_layout_}, {})) { return false; }

    const VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = std::uint32_t(shader_stage_create_infos.size()),
        .pStages = shader_stage_create_infos.data(),
        .pVertexInputState = &vertex_input_state_create_info,
        .pInputAssemblyState = &input_assembly_state_create_info,
        .pViewportState = &viewport_state_create_info,
        .pRasterizationState = &rasterization_state_create_info,
        .pMultisampleState = &multisample_state_create_info,
        .pDepthStencilState = render_target.useDepth() ? &depth_stencil_state_create_info : nullptr,
        .pColorBlendState = &blend_state_create_info,
        .pDynamicState = &dynamic_state_create_info,
        .layout = pipeline_layout_,
        .renderPass = render_target.getRenderPass(),
        .basePipelineIndex = -1,
    };

    VkResult result = vkCreateGraphicsPipelines(~device_, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr,
                                                &pipeline_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create graphics pipeline: {}", result);
        return false;
    }

    return true;
}

//@{ IPipeline

//@}

bool Pipeline::createDescriptorSetLayout(std::span<const VkDescriptorSetLayoutBinding> bindings) {
    const VkDescriptorSetLayoutCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = std::uint32_t(bindings.size()),
        .pBindings = bindings.data(),
    };

    VkResult result = vkCreateDescriptorSetLayout(~device_, &create_info, nullptr, &descriptor_set_layout_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create layout for descriptor sets: {}", result);
        return false;
    }

    return true;
}

bool Pipeline::createPipelineLayout(std::span<const VkDescriptorSetLayout> descriptor_set_layouts,
                                    std::span<const VkPushConstantRange> push_constant_ranges) {
    const VkPipelineLayoutCreateInfo pipeline_layout_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = std::uint32_t(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = std::uint32_t(push_constant_ranges.size()),
        .pPushConstantRanges = push_constant_ranges.data(),
    };

    VkResult result = vkCreatePipelineLayout(~device_, &pipeline_layout_create_info, nullptr, &pipeline_layout_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create pipeline layout: {}", result);
        return false;
    }

    return true;
}
