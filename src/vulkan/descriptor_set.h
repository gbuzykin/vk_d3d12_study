#pragma once

#include "pipeline_layout.h"

namespace app3d::rel::vulkan {

class Device;

class DescriptorSet final : public IDescriptorSet {
 public:
    DescriptorSet(Device& device, PipelineLayout& pipeline_layout);
    ~DescriptorSet() override;
    DescriptorSet(const DescriptorSet&) = delete;
    DescriptorSet& operator=(const DescriptorSet&) = delete;

    using BindingType = PipelineLayout::BindingType;

    bool create(std::uint32_t set_layout_index);

    VkDescriptorSet operator~() { return handle_.handle; }
    PipelineLayout& getLayout() { return pipeline_layout_; }

    //@{ IDescriptorSet
    void updateCombinedTextureSamplerDescriptor(ITexture& texture, ISampler& sampler, std::uint32_t slot) override;
    void updateConstantBufferDescriptor(IBuffer& buffer, std::uint32_t slot) override;
    //@}

 private:
    Device& device_;
    PipelineLayout& pipeline_layout_;
    PipelineLayout::DescriptorSetHandle handle_{};
};

}  // namespace app3d::rel::vulkan
