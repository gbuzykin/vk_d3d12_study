#pragma once

#include "d3d12_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::d3d12 {

class Device;

class Sampler final : public util::ref_counter, public ISampler {
 public:
    explicit Sampler(Device& device);
    ~Sampler() override;

    bool create(const SamplerDesc& desc);

    const D3D12_SAMPLER_DESC& getSamplerDesc() { return desc_; }

    //@{ ISampler
    util::ref_counter& getRefCounter() override { return *this; }
    //@}

 private:
    util::ref_ptr<Device> device_;
    D3D12_SAMPLER_DESC desc_;
};

}  // namespace app3d::rel::d3d12
