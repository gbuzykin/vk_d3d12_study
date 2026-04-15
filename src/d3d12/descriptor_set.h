#pragma once

#include "d3d12_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::d3d12 {

class Device;
class PipelineLayout;

class DescriptorSet final : public util::ref_counter, public IDescriptorSet {
 public:
    DescriptorSet(Device& device, PipelineLayout& pipeline_layout);
    ~DescriptorSet() override;

    bool create();

    PipelineLayout& getLayout() { return *pipeline_layout_; }

    //@{ IDescriptorSet
    util::ref_counter& getRefCounter() override { return *this; }
    void updateTextureSamplerDescriptor(ITexture& texture, ISampler& sampler, std::uint32_t slot) override;
    void updateConstantBufferDescriptor(IBuffer& buffer, std::uint32_t slot) override;
    //@}

 private:
    util::ref_ptr<Device> device_;
    util::ref_ptr<PipelineLayout> pipeline_layout_;
};

}  // namespace app3d::rel::d3d12
