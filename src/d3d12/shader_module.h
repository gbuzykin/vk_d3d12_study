#pragma once

#include "d3d12_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::d3d12 {

class Device;

class ShaderModule final : public util::ref_counter, public IShaderModule {
 public:
    explicit ShaderModule(Device& device);
    ~ShaderModule() override;

    bool create(std::span<const std::uint32_t> source);

    //@{ IShaderModule
    util::ref_counter& getRefCounter() override { return *this; }
    //@}

 private:
    util::ref_ptr<Device> device_;
};

}  // namespace app3d::rel::d3d12
