#pragma once

#include "d3d12_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::d3d12 {

class Device;
class RenderTarget;
class PipelineLayout;

class Pipeline final : public util::ref_counter, public IPipeline {
 public:
    Pipeline(Device& device, RenderTarget& render_target, PipelineLayout& pipeline_layout);
    ~Pipeline() override;

    bool create(std::span<IShaderModule* const> shader_modules, const uxs::db::value& config);

    PipelineLayout& getLayout() { return *pipeline_layout_; }

    //@{ IPipeline
    util::ref_counter& getRefCounter() override { return *this; }
    //@}

 private:
    util::ref_ptr<Device> device_;
    util::ref_ptr<RenderTarget> render_target_;
    util::ref_ptr<PipelineLayout> pipeline_layout_;
};

}  // namespace app3d::rel::d3d12
