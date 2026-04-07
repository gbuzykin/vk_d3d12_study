#include "sampler.h"

#include "device.h"
#include "object_destroyer.h"
#include "vulkan_logger.h"

#include <array>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Sampler class implementation

Sampler::Sampler(Device& device) : device_(util::not_null{&device}) {}

Sampler::~Sampler() { ObjectDestroyer<VkSampler>::destroy(~*device_, sampler_); }

bool Sampler::create(const SamplerDesc& desc) {
    constexpr std::array mag_filters{
        VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_FILTER_NEAREST,
        VK_FILTER_NEAREST, VK_FILTER_LINEAR,  VK_FILTER_LINEAR, VK_FILTER_LINEAR,
    };
    constexpr std::array min_filters{
        VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_FILTER_LINEAR,
        VK_FILTER_LINEAR,  VK_FILTER_LINEAR,  VK_FILTER_LINEAR,  VK_FILTER_LINEAR,
    };
    constexpr std::array mipmap_filters{
        VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR,  VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_MIPMAP_MODE_LINEAR,  VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR,
        VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR,  VK_SAMPLER_MIPMAP_MODE_LINEAR,
    };
    constexpr std::array address_mode{
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
    };

    const VkSamplerCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = mag_filters[unsigned(desc.filter)],
        .minFilter = min_filters[unsigned(desc.filter)],
        .mipmapMode = mipmap_filters[unsigned(desc.filter)],
        .addressModeU = address_mode[unsigned(desc.address_mode_u)],
        .addressModeV = address_mode[unsigned(desc.address_mode_v)],
        .addressModeW = address_mode[unsigned(desc.address_mode_w)],
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

    VkResult result = vkCreateSampler(~*device_, &create_info, nullptr, &sampler_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create sampler: {}", result);
        return false;
    }

    return true;
}

//@{ ISampler

//@}
