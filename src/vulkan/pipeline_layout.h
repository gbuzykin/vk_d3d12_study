#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::vulkan {

class Device;

class PipelineLayout : public IPipelineLayout {
 public:
    explicit PipelineLayout(Device& device);
    ~PipelineLayout();
    PipelineLayout(const PipelineLayout&) = delete;
    PipelineLayout& operator=(const PipelineLayout&) = delete;

    bool create(const uxs::db::value& config);

    VkPipelineLayout operator~() { return pipeline_layout_; }
    VkDescriptorSetLayout getDescriptorSetLayout() { return descriptor_set_layout_; }

    //@{ IPipelineLayout
    //@}

 private:
    Device& device_;
    VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};
    VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
