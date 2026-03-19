#include "sampler.h"

#include "device.h"
#include "object_destroyer.h"

#include "common/logger.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Sampler class implementation

Sampler::Sampler(Device& device) : device_(device) {}

Sampler::~Sampler() { ObjectDestroyer<VkSampler>::destroy(~device_, sampler_); }

bool Sampler::create(VkFilter mag_filter, VkFilter min_filter, VkSamplerMipmapMode mipmap_mode,
                     VkSamplerAddressMode u_address_mode, VkSamplerAddressMode v_address_mode,
                     VkSamplerAddressMode w_address_mode, float lod_bias, VkBool32 anisotropy_enable,
                     float max_anisotropy, VkBool32 compare_enable, VkCompareOp compare_operator, float min_lod,
                     float max_lod, VkBorderColor border_color, VkBool32 unnormalized_coords) {
    const VkSamplerCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = mag_filter,
        .minFilter = min_filter,
        .mipmapMode = mipmap_mode,
        .addressModeU = u_address_mode,
        .addressModeV = v_address_mode,
        .addressModeW = w_address_mode,
        .mipLodBias = lod_bias,
        .anisotropyEnable = anisotropy_enable,
        .maxAnisotropy = max_anisotropy,
        .compareEnable = compare_enable,
        .compareOp = compare_operator,
        .minLod = min_lod,
        .maxLod = max_lod,
        .borderColor = border_color,
        .unnormalizedCoordinates = unnormalized_coords,
    };

    VkResult result = vkCreateSampler(~device_, &create_info, nullptr, &sampler_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create sampler");
        return false;
    }

    return true;
}

//@{ ISampler

//@}
