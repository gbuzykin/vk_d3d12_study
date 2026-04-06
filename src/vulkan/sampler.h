#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::vulkan {

class Device;

class Sampler final : public util::ref_counter, public ISampler {
 public:
    explicit Sampler(Device& device);
    ~Sampler() override;

    bool create(VkFilter mag_filter, VkFilter min_filter, VkSamplerMipmapMode mipmap_mode,
                VkSamplerAddressMode u_address_mode, VkSamplerAddressMode v_address_mode,
                VkSamplerAddressMode w_address_mode, float lod_bias, VkBool32 anisotropy_enable, float max_anisotropy,
                VkBool32 compare_enable, VkCompareOp compare_operator, float min_lod, float max_lod,
                VkBorderColor border_color, VkBool32 unnormalized_coords);

    VkSampler operator~() { return sampler_; }

    //@{ ISampler
    util::ref_counter& getRefCounter() override { return *this; }
    //@}

 private:
    util::ref_ptr<Device> device_;
    VkSampler sampler_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
