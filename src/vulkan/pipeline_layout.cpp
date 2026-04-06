#include "pipeline_layout.h"

#include "device.h"
#include "enum_tables.h"
#include "object_destroyer.h"
#include "pipeline.h"
#include "render_target.h"
#include "shader_module.h"
#include "vulkan_logger.h"

#include <unordered_map>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// PipelineLayout class implementation

PipelineLayout::PipelineLayout(Device& device) : device_(device) {}

PipelineLayout::~PipelineLayout() {
    ObjectDestroyer<VkPipelineLayout>::destroy(~device_.get(), pipeline_layout_);
    for (const auto& ds_layout : descriptor_set_layouts_) {
        ObjectDestroyer<VkDescriptorSetLayout>::destroy(~device_.get(), ds_layout);
    }
}

namespace {
const std::unordered_map<VkDescriptorType, PipelineLayout::BindingType> g_binding_types{
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, PipelineLayout::BindingType::TEXTURE_SAMPLER},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, PipelineLayout::BindingType::CONSTANT_BUFFER},
};
}

bool PipelineLayout::create(const uxs::db::value& config) {
    const auto& descriptor_set_layouts = config.value("descriptor_set_layouts");
    descriptor_set_layouts_.reserve(descriptor_set_layouts.size());

    for (const auto& layout : descriptor_set_layouts.as_array()) {
        uxs::inline_dynarray<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(layout.size());

        const auto& list = layout.value("desc_list");
        std::array<std::uint32_t, unsigned(BindingType::COUNT)> slots{};
        for (const auto& desc : list.as_array()) {
            const std::uint32_t binding = desc.value<std::uint32_t>("binding");
            const auto type = parseDescriptorType(desc.value("type").as_string_view());
            const BindingType binding_type = g_binding_types.at(type);
            setBinding(binding_type, slots[unsigned(binding_type)]++, binding);

            const auto& stages = desc.value("stages");
            VkShaderStageFlags stage_flags = 0;
            for (const auto& stage_flag : stages.as_array()) {
                stage_flags |= parseShaderStage(stage_flag.as_string_view());
            }
            bindings.push_back(VkDescriptorSetLayoutBinding{
                .binding = binding,
                .descriptorType = type,
                .descriptorCount = desc.value<std::uint32_t>("count"),
                .stageFlags = stage_flags,
            });
        }

        const VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = std::uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
        VkResult result = vkCreateDescriptorSetLayout(~device_.get(), &create_info, nullptr, &descriptor_set_layout);
        if (result != VK_SUCCESS) {
            logError(LOG_VK "couldn't create layout for descriptor sets: {}", result);
            return false;
        }

        descriptor_set_layouts_.push_back(descriptor_set_layout);
    }

    uxs::inline_dynarray<VkPushConstantRange> push_constant_ranges;

    const VkPipelineLayoutCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = std::uint32_t(descriptor_set_layouts_.size()),
        .pSetLayouts = descriptor_set_layouts_.data(),
        .pushConstantRangeCount = std::uint32_t(push_constant_ranges.size()),
        .pPushConstantRanges = push_constant_ranges.data(),
    };

    VkResult result = vkCreatePipelineLayout(~device_.get(), &create_info, nullptr, &pipeline_layout_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create pipeline layout: {}", result);
        return false;
    }

    return true;
}

//@{ IPipelineLayout

//@}
