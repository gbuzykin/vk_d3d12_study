#include "descriptor_set.h"

#include "device.h"
#include "sampler.h"
#include "texture.h"

#include <array>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// DescriptorSet class implementation

DescriptorSet::DescriptorSet(Device& device) : device_(device) {}

DescriptorSet::~DescriptorSet() {}

bool DescriptorSet::create(VkDescriptorSetLayout descriptor_set_layout) {
    return device_.obtainDescriptorSet(descriptor_set_layout, descriptor_set_);
}

//@{ IDescriptorSet

void DescriptorSet::updateCombinedTextureSamplerDescriptor(ITexture& texture, ISampler& sampler) {
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

//@}
