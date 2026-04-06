#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::vulkan {

class Device;

class ShaderModule final : public util::ref_counter, public IShaderModule {
 public:
    explicit ShaderModule(Device& device);
    ~ShaderModule() override;

    bool create(std::span<const std::uint32_t> source);

    VkShaderModule operator~() { return shader_module_; }

    //@{ IUnknown
    util::ref_counter& getRefCounter() override { return *this; }
    //@}

    //@{ IShaderModule
    //@}

 private:
    util::reference<Device> device_;
    VkShaderModule shader_module_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
