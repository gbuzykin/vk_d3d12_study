#pragma once

#include "frame_image_provider.h"

namespace app3d::rel::d3d12 {

class Device;

class Texture final : public FrameImageProvider, public ITexture {
 public:
    explicit Texture(Device& device);
    ~Texture() override;

    bool create(const TextureOpts& opts);

    //@{ FrameImageProvider
    ID3D12Resource* getImageResource(std::uint32_t image_index) override { return nullptr; }
    std::uint32_t getImageCount() const override { return 0; }
    std::uint32_t getFifCount() const override { return 0; }
    Extent2u getImageExtent() const override { return {}; }
    DXGI_FORMAT getImageFormat() const override { return DXGI_FORMAT_UNKNOWN; }
    void removeRenderTarget(RenderTarget* render_target) override {}
    //@}

    //@{ ITexture
    util::ref_counter& getRefCounter() override { return *this; }
    bool updateTexture(std::span<const std::uint8_t> data, Vec3i offset, Extent3u extent) override;
    util::ref_ptr<IRenderTarget> createRenderTarget(const uxs::db::value& opts) override;
    //@}

 private:
    util::ref_ptr<Device> device_;
};

}  // namespace app3d::rel::d3d12
