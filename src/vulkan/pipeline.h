#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::vulkan {

class Device;
class PipelineLayout;
class RenderTarget;

class Pipeline final : public IPipeline {
 public:
    Pipeline(Device& device, PipelineLayout& pipeline_layout);
    ~Pipeline() override;
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    bool create(RenderTarget& render_target, std::span<IShaderModule* const> shader_modules,
                const uxs::db::value& config);

    VkPipeline operator~() { return pipeline_; }
    PipelineLayout& getLayout() { return pipeline_layout_; }

    //@{ IPipeline
    //@}

 private:
    Device& device_;
    PipelineLayout& pipeline_layout_;
    VkPipeline pipeline_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
