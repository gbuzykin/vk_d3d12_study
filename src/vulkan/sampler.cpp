#include "sampler.h"

#include "device.h"
#include "tables.h"
#include "vulkan_logger.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Sampler class implementation

Sampler::Sampler(Device& device) : device_(util::not_null{&device}) {}

Sampler::~Sampler() { device_->vkDestroySampler(sampler_, nullptr); }

bool Sampler::create(const SamplerDesc& desc) {
    const VkSamplerCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = TBL_VK_MIN_MAG_FILTER[unsigned(desc.filter)][1],
        .minFilter = TBL_VK_MIN_MAG_FILTER[unsigned(desc.filter)][0],
        .mipmapMode = TBL_VK_MIPMAP_FILTER[unsigned(desc.filter)],
        .addressModeU = TBL_VK_ADDRESS_MODE[unsigned(desc.address_mode_u)],
        .addressModeV = TBL_VK_ADDRESS_MODE[unsigned(desc.address_mode_v)],
        .addressModeW = TBL_VK_ADDRESS_MODE[unsigned(desc.address_mode_w)],
        .mipLodBias = desc.mip_lod_bias,
        .anisotropyEnable = desc.filter >= SamplerFilter::ANISOTROPIC ? VK_TRUE : VK_FALSE,
        .maxAnisotropy = float(desc.max_anisotropy),
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = desc.min_lod,
        .maxLod = desc.max_lod,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    VkResult result = device_->vkCreateSampler(&create_info, nullptr, &sampler_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create sampler: {}", result);
        return false;
    }

    return true;
}

//@{ ISampler

//@}
