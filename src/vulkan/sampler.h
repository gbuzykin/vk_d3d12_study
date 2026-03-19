#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::vulkan {

class Device;

class Sampler final : public ISampler {
 public:
    explicit Sampler(Device& device);
    ~Sampler() override;
    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;

    bool create(VkFilter mag_filter, VkFilter min_filter, VkSamplerMipmapMode mipmap_mode,
                VkSamplerAddressMode u_address_mode, VkSamplerAddressMode v_address_mode,
                VkSamplerAddressMode w_address_mode, float lod_bias, bool anisotropy_enable, float max_anisotropy,
                bool compare_enable, VkCompareOp compare_operator, float min_lod, float max_lod,
                VkBorderColor border_color, bool unnormalized_coords);

    VkSampler operator~() { return sampler_; }

    //@{ ISampler
    //@}

 private:
    Device& device_;
    VkSampler sampler_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
