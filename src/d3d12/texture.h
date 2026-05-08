#pragma once

#include "frame_image_provider.h"

namespace app3d::rel::d3d12 {

class Device;

class Texture final : public FrameImageProvider, public ITexture {
 public:
    explicit Texture(Device& device);
    ~Texture() override;

    bool create(const TextureDesc& desc);

    const D3D12_SHADER_RESOURCE_VIEW_DESC& getShaderResourceViewDesc() const { return view_desc_; }
    ID3D12Resource* getResource() { return allocation_->GetResource(); }

    //@{ FrameImageProvider
    ID3D12Resource* getImageResource(std::uint32_t image_index) override { return allocation_->GetResource(); }
    std::uint32_t getImageCount() const override { return 1; }
    std::uint32_t getFifCount() const override { return 1; }
    Format getImageFormat() const override { return image_format_; }
    Extent2u getImageExtent() const override { return Extent2u(image_extent_); }
    void imageBarrierBefore(ID3D12GraphicsCommandList* command_list, std::uint32_t image_index) override {}
    void imageBarrierAfter(ID3D12GraphicsCommandList* command_list, std::uint32_t image_index) override {}
    RenderTargetResult presentImage() { return RenderTargetResult::SUCCESS; }
    void removeRenderTarget(RenderTarget* render_target) override {}
    //@}

    //@{ ITexture
    util::ref_counter& getRefCounter() override { return *this; }
    bool updateTexture(const std::uint8_t* data, std::uint32_t first_subresource,
                       std::span<const UpdateTextureDesc> update_subresource_descs) override;
    util::ref_ptr<IRenderTarget> createRenderTarget(const uxs::db::value& opts) override;
    //@}

 private:
    util::ref_ptr<Device> device_;
    util::ref_ptr<D3D12MA::Allocation> allocation_;
    Format image_format_{};
    Extent3u image_extent_{};
    D3D12_RESOURCE_DESC desc_;
    D3D12_SHADER_RESOURCE_VIEW_DESC view_desc_;
};

}  // namespace app3d::rel::d3d12
