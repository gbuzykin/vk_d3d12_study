#pragma once

#include "pipeline_layout.h"

namespace app3d::rel::d3d12 {

class DescriptorSet final : public util::ref_counter, public IDescriptorSet {
 public:
    DescriptorSet(Device& device, PipelineLayout& pipeline_layout);
    ~DescriptorSet() override;

    using HeapType = PipelineLayout::HeapType;

    bool create(std::uint32_t set_layout_index) {
        return pipeline_layout_->obtainDescriptorSet(set_layout_index, handles_);
    }

    PipelineLayout& getLayout() { return *pipeline_layout_; }
    const PipelineLayout::DescriptorSetHandles& getDescriptorHandles() { return handles_; }

    //@{ IDescriptorSet
    util::ref_counter& getRefCounter() override { return *this; }
    void updateCombinedTextureSamplerDescriptor(ITexture& texture, ISampler& sampler, std::uint32_t slot) override;
    void updateConstantBufferDescriptor(IBuffer& buffer, std::uint64_t offset, std::uint64_t size,
                                        std::uint32_t slot) override;
    //@}

 private:
    util::ref_ptr<Device> device_;
    util::ref_ptr<PipelineLayout> pipeline_layout_;
    PipelineLayout::DescriptorSetHandles handles_{};
};

}  // namespace app3d::rel::d3d12
