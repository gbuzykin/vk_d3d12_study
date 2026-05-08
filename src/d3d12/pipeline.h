#pragma once

#include "d3d12_api.h"

#include "interfaces/i_rendering_driver.h"

#include <uxs/dynarray.h>

namespace app3d::rel::d3d12 {

class Device;
class RenderTarget;
class PipelineLayout;

class Pipeline final : public util::ref_counter, public IPipeline {
 public:
    Pipeline(Device& device, RenderTarget& render_target, PipelineLayout& pipeline_layout);
    ~Pipeline() override;

    std::uint32_t getVertexStride(std::uint32_t slot) const { return vertex_strides_[slot]; }

    bool create(std::span<IShaderModule* const> shader_modules, const uxs::db::value& config);

    ID3D12PipelineState* getD3D12PipelineState() { return pipeline_.get(); }
    PipelineLayout& getLayout() { return *pipeline_layout_; }

    //@{ IPipeline
    util::ref_counter& getRefCounter() override { return *this; }
    //@}

 private:
    util::ref_ptr<Device> device_;
    util::ref_ptr<RenderTarget> render_target_;
    util::ref_ptr<PipelineLayout> pipeline_layout_;
    util::ref_ptr<ID3D12PipelineState> pipeline_;

    uxs::inline_dynarray<std::uint32_t> vertex_strides_;
};

}  // namespace app3d::rel::d3d12
