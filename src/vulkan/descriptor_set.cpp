#include "descriptor_set.h"

#include "device.h"
#include "sampler.h"
#include "texture.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// DescriptorSet class implementation

DescriptorSet::DescriptorSet(Device& device, PipelineLayout& pipeline_layout)
    : device_(device), pipeline_layout_(pipeline_layout) {}

DescriptorSet::~DescriptorSet() {}

bool DescriptorSet::create(std::uint32_t set_layout_index) {
    return pipeline_layout_.obtainDescriptorSet(set_layout_index, handle_);
}

//@{ IDescriptorSet

void DescriptorSet::updateCombinedTextureSamplerDescriptor(ITexture& texture, ISampler& sampler, std::uint32_t slot) {
    const auto& binding = handle_.bindings[slot][unsigned(BindingType::SHADER_RESOURCE)];
    const std::array image_infos{
        VkDescriptorImageInfo{
            .sampler = ~static_cast<Sampler&>(sampler),
            .imageView = static_cast<Texture&>(texture).getImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
    };
    device_.updateDescriptorSets(
        std::array{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = handle_.handle,
                .dstBinding = binding.binding,
                .dstArrayElement = binding.array_element,
                .descriptorCount = std::uint32_t(image_infos.size()),
                .descriptorType = PipelineLayout::vulkan_desc_types[unsigned(binding.type)],
                .pImageInfo = image_infos.data(),
            },
        },
        {});
}

void DescriptorSet::updateConstantBufferDescriptor(IBuffer& buffer, std::uint32_t slot) {
    const auto& binding = handle_.bindings[slot][unsigned(BindingType::CONSTANT_BUFFER)];
    const std::array buffer_infos{
        VkDescriptorBufferInfo{
            .buffer = ~static_cast<Buffer&>(buffer),
            .offset = 0,
            .range = VK_WHOLE_SIZE,
        },
    };
    device_.updateDescriptorSets(
        std::array{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = handle_.handle,
                .dstBinding = binding.binding,
                .dstArrayElement = binding.array_element,
                .descriptorCount = std::uint32_t(buffer_infos.size()),
                .descriptorType = PipelineLayout::vulkan_desc_types[unsigned(binding.type)],
                .pBufferInfo = buffer_infos.data(),
            },
        },
        {});
}

//@}
