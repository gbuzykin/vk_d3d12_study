#include "pipeline_layout.h"

#include "device.h"
#include "object_destroyer.h"
#include "pipeline.h"
#include "render_target.h"
#include "shader_module.h"
#include "vulkan_logger.h"

#include <array>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// PipelineLayout class implementation

PipelineLayout::PipelineLayout(Device& device) : device_(device) {}

PipelineLayout::~PipelineLayout() {
    ObjectDestroyer<VkPipelineLayout>::destroy(~device_, pipeline_layout_);
    ObjectDestroyer<VkDescriptorSetLayout>::destroy(~device_, descriptor_set_layout_);
}

bool PipelineLayout::create(const uxs::db::value& config) {
    uxs::inline_dynarray<VkDescriptorSetLayoutBinding> set_layout_bindings;

    set_layout_bindings.emplace_back(VkDescriptorSetLayoutBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    });

    const VkDescriptorSetLayoutCreateInfo set_create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = std::uint32_t(set_layout_bindings.size()),
        .pBindings = set_layout_bindings.data(),
    };

    VkResult result = vkCreateDescriptorSetLayout(~device_, &set_create_info, nullptr, &descriptor_set_layout_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create layout for descriptor sets: {}", result);
        return false;
    }

    const std::array descriptor_set_layouts{descriptor_set_layout_};

    uxs::inline_dynarray<VkPushConstantRange> push_constant_ranges;

    const VkPipelineLayoutCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = std::uint32_t(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = std::uint32_t(push_constant_ranges.size()),
        .pPushConstantRanges = push_constant_ranges.data(),
    };

    result = vkCreatePipelineLayout(~device_, &create_info, nullptr, &pipeline_layout_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create pipeline layout: {}", result);
        return false;
    }

    return true;
}

//@{ IPipelineLayout

//@}
