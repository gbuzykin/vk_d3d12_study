#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::vulkan {

class Device;

class DescriptorSet final : public IDescriptorSet {
 public:
    explicit DescriptorSet(Device& device);
    ~DescriptorSet() override;
    DescriptorSet(const DescriptorSet&) = delete;
    DescriptorSet& operator=(const DescriptorSet&) = delete;

    bool create(VkDescriptorSetLayout descriptor_set_layout);

    VkDescriptorSet operator~() { return descriptor_set_; }

    //@{ IDescriptorSet
    void updateCombinedTextureSamplerDescriptor(ITexture& texture, ISampler& sampler) override;
    //@}

 private:
    Device& device_;
    VkDescriptorSet descriptor_set_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
