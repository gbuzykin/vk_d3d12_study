#include "pipeline_layout.h"

#include "descriptor_set.h"
#include "device.h"
#include "pipeline.h"
#include "render_target.h"
#include "shader_module.h"
#include "tables.h"
#include "vulkan_logger.h"

#include "rel/tables.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// PipelineLayout class implementation

PipelineLayout::PipelineLayout(Device& device) : device_(util::not_null(&device)) {}

PipelineLayout::~PipelineLayout() {
    device_->vkDestroyDescriptorPool(desc_pool_, nullptr);
    device_->vkDestroyPipelineLayout(pipeline_layout_, nullptr);
    for (const auto& set_layout : set_layouts_) { device_->vkDestroyDescriptorSetLayout(set_layout, nullptr); }
}

bool PipelineLayout::create(const uxs::db::value& config) {
    std::uint32_t total_max_sets = 0;
    uxs::inline_dynarray<VkDescriptorPoolSize> desc_counts;

    const auto& descriptor_set_layouts = config.value("descriptor_set_layouts");

    for (const auto& layout : descriptor_set_layouts.as_array()) {
        uxs::inline_dynarray<VkDescriptorSetLayoutBinding> bindings;
        const std::uint32_t max_sets = layout.value_or<std::uint32_t>("max_sets", 8);
        total_max_sets += max_sets;

        const auto& list = layout.value("descriptor_list");

        std::uint32_t def_binding = 0;
        std::array<std::uint32_t, unsigned(BindingType::TOTAL_COUNT)> next_slots{};

        const std::uint32_t binding_offset = std::uint32_t(bindings_.size());

        for (const auto& desc : list.as_array()) {
            const std::uint32_t binding = desc.value_or<std::uint32_t>("binding", def_binding);
            const auto type = parseDescriptorType(desc.value("type").as_string_view());
            const auto binding_type = TBL_DESC_BINDING_TYPE[unsigned(type)];
            const auto vulkan_type = TBL_VK_DESC_TYPE[unsigned(type)];
            const std::uint32_t slot = desc.value_or<std::uint32_t>("slot", next_slots[unsigned(binding_type)]);
            const std::uint32_t desc_count = desc.value_or<std::uint32_t>("count", 1);
            def_binding = binding + 1;

            setBinding(binding_type, binding_offset + slot, type, binding, desc_count);
            next_slots[unsigned(binding_type)] = slot + desc_count;
            if (type == DescriptorType::COMBINED_TEXTURE_SAMPLER) {
                setBinding(BindingType::SAMPLER, binding_offset + slot, type, binding, desc_count);
                next_slots[unsigned(BindingType::SAMPLER)] = slot + desc_count;
            }

            const auto& stages = desc.value("shader_visibility");
            const auto visibility = parseShaderStage(desc.value_or<const char*>("shader_visibility", "ALL"));
            bindings.emplace_back(VkDescriptorSetLayoutBinding{
                .binding = binding,
                .descriptorType = vulkan_type,
                .descriptorCount = desc_count,
                .stageFlags = VkShaderStageFlags(TBL_VK_SHADER_STAGE[unsigned(visibility)]),
            });

            auto desc_counts_it = std::ranges::find_if(
                desc_counts, [vulkan_type](const auto& item) { return item.type == vulkan_type; });
            if (desc_counts_it != desc_counts.end()) {
                desc_counts_it->descriptorCount += max_sets * desc_count;
            } else {
                desc_counts.emplace_back(VkDescriptorPoolSize{
                    .type = vulkan_type,
                    .descriptorCount = max_sets * desc_count,
                });
            }
        }

        const VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = std::uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
        VkResult result = device_->vkCreateDescriptorSetLayout(&create_info, nullptr, &set_layout);
        if (result != VK_SUCCESS) {
            logError(LOG_VK "couldn't create layout for descriptor sets: {}", result);
            return false;
        }

        set_layouts_.push_back(set_layout);
        binding_offsets_.push_back(binding_offset);
    }

    const VkPipelineLayoutCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = std::uint32_t(set_layouts_.size()),
        .pSetLayouts = set_layouts_.data(),
    };

    VkResult result = device_->vkCreatePipelineLayout(&create_info, nullptr, &pipeline_layout_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create pipeline layout: {}", result);
        return false;
    }

    return createDescriptorPool(total_max_sets, desc_counts);
}

bool PipelineLayout::obtainDescriptorSet(std::uint32_t set_layout_index, DescriptorSetHandle& handle) {
    const VkDescriptorSetAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = desc_pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &set_layouts_[set_layout_index],
    };

    VkResult result = device_->vkAllocateDescriptorSets(&allocate_info, &handle.handle);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't allocate descriptor sets: {}", result);
        return false;
    }

    handle.bindings = &bindings_[binding_offsets_[set_layout_index]];
    return true;
}

//@{ IPipelineLayout

util::ref_ptr<IDescriptorSet> PipelineLayout::createDescriptorSet(std::uint32_t set_layout_index) {
    auto descriptor_set = util::make_new<DescriptorSet>(*device_, *this);
    if (!descriptor_set->create(set_layout_index)) { return nullptr; }
    return std::move(descriptor_set);
}

void PipelineLayout::resetDescriptorAllocator() { device_->vkResetDescriptorPool(desc_pool_, 0); }

//@}

bool PipelineLayout::createDescriptorPool(std::uint32_t total_max_sets,
                                          std::span<const VkDescriptorPoolSize> desc_counts) {
    const VkDescriptorPoolCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = total_max_sets,
        .poolSizeCount = std::uint32_t(desc_counts.size()),
        .pPoolSizes = desc_counts.data(),
    };

    VkResult result = device_->vkCreateDescriptorPool(&create_info, nullptr, &desc_pool_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create descriptor pool: {}", result);
        return false;
    }

    return true;
}
