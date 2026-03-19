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
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    bool create(RenderTarget& render_target, std::span<IShaderModule* const> shader_modules,
                const uxs::db::value& config);

    VkPipeline operator~() { return pipeline_; }
    VkDescriptorSetLayout getDescriptorSetLayout() { return descriptor_set_layout_; }
    VkPipelineLayout getLayout() { return pipeline_layout_; }

    //@{ IPipeline
    //@}

 private:
    Device& device_;
    VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};
    VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};

    bool createDescriptorSetLayout(std::span<const VkDescriptorSetLayoutBinding> bindings);
    bool createPipelineLayout(std::span<const VkDescriptorSetLayout> descriptor_set_layouts,
                              std::span<const VkPushConstantRange> push_constant_ranges);
};

}  // namespace app3d::rel::vulkan
