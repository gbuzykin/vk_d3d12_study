#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::vulkan {

class Device;

class ShaderModule final : public IShaderModule {
 public:
    explicit ShaderModule(Device& device);
    ~ShaderModule() override;
    ShaderModule(const ShaderModule&) = delete;
    ShaderModule& operator=(const ShaderModule&) = delete;

    bool create(std::span<const std::uint8_t> source_spirv, const uxs::db::value& create_info);

    VkShaderModule operator~() { return shader_module_; }

    //@{ IShaderModule
    //@}

 private:
    Device& device_;
    VkShaderModule shader_module_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
