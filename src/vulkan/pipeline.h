#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::vulkan {

class Device;
class RenderTarget;

class Pipeline final : public IPipeline {
 public:
    explicit Pipeline(Device& device);
    ~Pipeline() override;

    bool create(RenderTarget& render_target, std::span<const VkPipelineShaderStageCreateInfo> shader_stage_create_infos,
                const uxs::db::value& create_info);

    VkPipeline operator~() { return pipeline_; }

    //@{ IPipeline
    //@}

 private:
    Device& device_;
    VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
