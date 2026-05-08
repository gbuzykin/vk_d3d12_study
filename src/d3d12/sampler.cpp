#include "sampler.h"

#include "device.h"
#include "tables.h"

#include <array>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// Sampler class implementation

Sampler::Sampler(Device& device) : device_(util::not_null{&device}) {}

Sampler::~Sampler() {}

bool Sampler::create(const SamplerDesc& desc) {
    desc_ = {
        .Filter = TBL_D3D12_FILTER[unsigned(desc.filter)],
        .AddressU = TBL_D3D12_ADDRESS_MODE[unsigned(desc.address_mode_u)],
        .AddressV = TBL_D3D12_ADDRESS_MODE[unsigned(desc.address_mode_u)],
        .AddressW = TBL_D3D12_ADDRESS_MODE[unsigned(desc.address_mode_u)],
        .MipLODBias = FLOAT(desc.mip_lod_bias),
        .MaxAnisotropy = desc.filter >= SamplerFilter::ANISOTROPIC ? 1U : UINT(desc.max_anisotropy),
        .ComparisonFunc = D3D12_COMPARISON_FUNC_NONE,
        .MinLOD = FLOAT(desc.min_lod),
        .MaxLOD = FLOAT(desc.max_lod),
    };

    return true;
}

//@{ ISampler

//@}
