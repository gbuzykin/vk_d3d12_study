#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::vulkan {

class Device;
class PipelineLayout;

class DescriptorSet final : public IDescriptorSet {
 public:
    DescriptorSet(Device& device, PipelineLayout& pipeline_layout);
    ~DescriptorSet() override;
    DescriptorSet(const DescriptorSet&) = delete;
    DescriptorSet& operator=(const DescriptorSet&) = delete;

    bool create();

    VkDescriptorSet operator~() { return descriptor_set_; }
    PipelineLayout& getLayout() { return pipeline_layout_; }

    //@{ IDescriptorSet
    void updateCombinedTextureSamplerDescriptor(ITexture& texture, ISampler& sampler) override;
    void updateConstantBufferDescriptor(IBuffer& buffer) override;
    //@}

 private:
    Device& device_;
    PipelineLayout& pipeline_layout_;
    VkDescriptorSet descriptor_set_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
