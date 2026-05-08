#include "texture.h"

#include "d3d12_logger.h"
#include "device.h"
#include "render_target.h"
#include "tables.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// Texture class implementation

Texture::Texture(Device& device) : device_(util::not_null{&device}) {}

Texture::~Texture() {}

bool Texture::create(const TextureDesc& desc) {
    const std::uint32_t num_mipmaps = 1;

    image_format_ = desc.format;
    image_extent_ = desc.extent;

    const D3D12_RESOURCE_FLAGS flags = (!!(desc.flags & TextureFlags::RENDER_TARGET) ?
                                            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET :
                                            D3D12_RESOURCE_FLAG_NONE);

    const auto d3d12_image_format = TBL_D3D12_FORMAT[unsigned(image_format_)];

    const D3D12MA::ALLOCATION_DESC allocation_desc = {
        .HeapType = D3D12_HEAP_TYPE_DEFAULT,
    };

    desc_ = D3D12_RESOURCE_DESC{
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = UINT64(desc.extent.width),
        .Height = UINT(desc.extent.height),
        .DepthOrArraySize = 1,
        .MipLevels = UINT16(num_mipmaps),
        .Format = d3d12_image_format,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = flags,
    };

    HRESULT result = device_->getAllocator()->CreateResource(&allocation_desc, &desc_, D3D12_RESOURCE_STATE_COMMON,
                                                             nullptr, allocation_.reset_and_get_address(), IID_NULL,
                                                             nullptr);
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create texture resource: {}", D3D12Result(result));
        return false;
    }

    view_desc_ = {
        .Format = d3d12_image_format,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D =
            {
                .MostDetailedMip = 0,
                .MipLevels = UINT(num_mipmaps),
                .ResourceMinLODClamp = 0.0f,
            },
    };

    return true;
}

//@{ ITexture

bool Texture::updateTexture(const std::uint8_t* data, std::uint32_t first_subresource,
                            std::span<const UpdateTextureDesc> update_subresource_descs) {
    return device_->updateTexture(data, allocation_->GetResource(), image_format_, desc_, first_subresource,
                                  update_subresource_descs, D3D12_RESOURCE_STATE_COMMON,
                                  D3D12_RESOURCE_STATE_GENERIC_READ);
}

util::ref_ptr<IRenderTarget> Texture::createRenderTarget(const uxs::db::value& opts) {
    auto render_target = util::make_new<RenderTarget>(*device_, *this);
    if (!render_target->create(opts)) { return nullptr; }
    return std::move(render_target);
}

//@}
