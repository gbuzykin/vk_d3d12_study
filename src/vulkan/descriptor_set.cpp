#include "descriptor_set.h"

#include "device.h"
#include "sampler.h"
#include "tables.h"
#include "texture.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// DescriptorSet class implementation

DescriptorSet::DescriptorSet(Device& device, PipelineLayout& pipeline_layout)
    : device_(util::not_null{&device}), pipeline_layout_(util::not_null{&pipeline_layout}) {}

DescriptorSet::~DescriptorSet() {}

//@{ IDescriptorSet

void DescriptorSet::updateCombinedTextureSamplerDescriptor(ITexture& texture, ISampler& sampler, std::uint32_t slot) {
    const auto& binding = pipeline_layout_->getBinding(
        (*handle_.binding_offsets)[unsigned(BindingType::SHADER_RESOURCE)] + slot);
    const std::array image_infos{
        VkDescriptorImageInfo{
            .sampler = static_cast<Sampler&>(sampler).getHandle(),
            .imageView = static_cast<Texture&>(texture).getImageView(0),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
    };
    device_->updateDescriptorSets(
        std::array{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = handle_.handle,
                .dstBinding = binding.binding,
                .dstArrayElement = binding.array_element,
                .descriptorCount = std::uint32_t(image_infos.size()),
                .descriptorType = TBL_VK_DESC_TYPE[unsigned(binding.desc_type)],
                .pImageInfo = image_infos.data(),
            },
        },
        {});
}

void DescriptorSet::updateConstantBufferDescriptor(IBuffer& buffer, std::uint64_t offset, std::uint64_t size,
                                                   std::uint32_t slot) {
    const auto& binding = pipeline_layout_->getBinding(
        (*handle_.binding_offsets)[unsigned(BindingType::CONSTANT_BUFFER)] + slot);
    const std::array buffer_infos{
        VkDescriptorBufferInfo{
            .buffer = static_cast<Buffer&>(buffer).getHandle(),
            .offset = VkDeviceSize(offset),
            .range = VkDeviceSize(size),
        },
    };
    device_->updateDescriptorSets(
        std::array{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = handle_.handle,
                .dstBinding = binding.binding,
                .dstArrayElement = binding.array_element,
                .descriptorCount = std::uint32_t(buffer_infos.size()),
                .descriptorType = TBL_VK_DESC_TYPE[unsigned(binding.desc_type)],
                .pBufferInfo = buffer_infos.data(),
            },
        },
        {});
}

//@}
