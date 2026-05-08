#pragma once

#include "d3d12_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::d3d12 {

class Device;

class ShaderModule final : public util::ref_counter, public IShaderModule {
 public:
    explicit ShaderModule(Device& device);
    ~ShaderModule() override;

    const DataBlob& getBytecode() const { return bytecode_; }

    bool create(DataBlob bytecode) {
        bytecode_ = std::move(bytecode);
        return true;
    }

    //@{ IShaderModule
    util::ref_counter& getRefCounter() override { return *this; }
    //@}

 private:
    util::ref_ptr<Device> device_;
    DataBlob bytecode_;
};

}  // namespace app3d::rel::d3d12
