#include "descriptor_set.h"

#include "device.h"
#include "pipeline_layout.h"
#include "sampler.h"
#include "texture.h"

#include <array>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// DescriptorSet class implementation

DescriptorSet::DescriptorSet(Device& device, PipelineLayout& pipeline_layout)
    : device_(device), pipeline_layout_(pipeline_layout) {}

DescriptorSet::~DescriptorSet() { device_.releaseDescriptorSet(descriptor_set_); }

bool DescriptorSet::create() {
    return device_.obtainDescriptorSet(pipeline_layout_.getDescriptorSetLayout(), descriptor_set_);
}

//@{ IDescriptorSet

void DescriptorSet::updateTextureSamplerDescriptor(ITexture& texture, ISampler& sampler) {
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
                .dstSet = descriptor_set_,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = std::uint32_t(image_infos.size()),
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = image_infos.data(),
            },
        },
        {});
}

void DescriptorSet::updateConstantBufferDescriptor(IBuffer& buffer) {
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
                .dstSet = descriptor_set_,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = std::uint32_t(buffer_infos.size()),
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = buffer_infos.data(),
            },
        },
        {});
}

//@}
