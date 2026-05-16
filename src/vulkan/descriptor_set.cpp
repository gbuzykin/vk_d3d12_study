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

void DescriptorSet::updateSamplerDescriptor(ISampler& sampler, std::uint32_t slot) {
    const auto& binding_offsets = *handle_.binding_offsets;
    writeImageDescriptorSet(pipeline_layout_->getBinding(binding_offsets, BindingType::SAMPLER, slot),
                            std::array{VkDescriptorImageInfo{.sampler = static_cast<Sampler&>(sampler).getHandle()}});
}

void DescriptorSet::updateCombinedTextureSamplerDescriptor(ITexture& texture, ISampler& sampler, std::uint32_t slot,
                                                           std::uint32_t sampler_slot) {
    const auto& binding_offsets = *handle_.binding_offsets;
    const auto& binding = pipeline_layout_->getBinding(binding_offsets, BindingType::SHADER_RESOURCE, slot);
    assert(pipeline_layout_->getBinding(binding_offsets, BindingType::SAMPLER, slot).binding == binding.binding);
    writeImageDescriptorSet(binding, std::array{VkDescriptorImageInfo{
                                         .sampler = static_cast<Sampler&>(sampler).getHandle(),
                                         .imageView = static_cast<Texture&>(texture).getImageView(0),
                                         .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     }});
}

void DescriptorSet::updateShaderResourceDescriptor(ITexture& texture, std::uint32_t slot) {
    const auto& binding_offsets = *handle_.binding_offsets;
    writeImageDescriptorSet(pipeline_layout_->getBinding(binding_offsets, BindingType::SHADER_RESOURCE, slot),
                            std::array{VkDescriptorImageInfo{
                                .imageView = static_cast<Texture&>(texture).getImageView(0),
                                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            }});
}

void DescriptorSet::updateConstantBufferDescriptor(IBuffer& buffer, std::uint64_t offset, std::uint64_t size,
                                                   std::uint32_t slot) {
    const auto& binding_offsets = *handle_.binding_offsets;
    writeBufferDescriptorSet(pipeline_layout_->getBinding(binding_offsets, BindingType::CONSTANT_BUFFER, slot),
                             std::array{VkDescriptorBufferInfo{
                                 .buffer = static_cast<Buffer&>(buffer).getHandle(),
                                 .offset = VkDeviceSize(offset),
                                 .range = VkDeviceSize(size),
                             }});
}

//@}

void DescriptorSet::writeImageDescriptorSet(PipelineLayout::Binding binding,
                                            std::span<const VkDescriptorImageInfo> image_infos) {
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

void DescriptorSet::writeBufferDescriptorSet(PipelineLayout::Binding binding,
                                             std::span<const VkDescriptorBufferInfo> buffer_infos) {
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
