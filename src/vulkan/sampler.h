#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::vulkan {

class Device;

class Sampler final : public util::ref_counter, public ISampler {
 public:
    explicit Sampler(Device& device);
    ~Sampler() override;

    bool create(const SamplerDesc& desc);

    VkSampler getHandle() { return sampler_; }

    //@{ ISampler
    util::ref_counter& getRefCounter() override { return *this; }
    //@}

 private:
    util::ref_ptr<Device> device_;
    VkSampler sampler_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
