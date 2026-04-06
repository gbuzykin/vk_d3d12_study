#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::vulkan {

class Device;
class PipelineLayout;

class DescriptorSet final : public util::ref_counter, public IDescriptorSet {
 public:
    DescriptorSet(Device& device, PipelineLayout& pipeline_layout);
    ~DescriptorSet() override;

    bool create();

    VkDescriptorSet operator~() { return descriptor_set_; }
    PipelineLayout& getLayout() { return pipeline_layout_; }

    //@{ IUnknown
    util::ref_counter& getRefCounter() override { return *this; }
    //@}

    //@{ IDescriptorSet
    void updateTextureSamplerDescriptor(ITexture& texture, ISampler& sampler, std::uint32_t slot) override;
    void updateConstantBufferDescriptor(IBuffer& buffer, std::uint32_t slot) override;
    //@}

 private:
    util::reference<Device> device_;
    util::reference<PipelineLayout> pipeline_layout_;
    VkDescriptorSet descriptor_set_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
