#pragma once

#include "d3d12_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::d3d12 {

class RenderTarget;

class FrameImageProvider : public util::ref_counter {
 public:
    virtual ID3D12Resource* getImageResource(std::uint32_t image_index) = 0;
    virtual std::uint32_t getImageCount() const = 0;
    virtual std::uint32_t getFifCount() const = 0;
    virtual Format getImageFormat() const = 0;
    virtual Extent2u getImageExtent() const = 0;
    virtual void imageBarrierBefore(ID3D12GraphicsCommandList* command_list, std::uint32_t image_index) = 0;
    virtual void imageBarrierAfter(ID3D12GraphicsCommandList* command_list, std::uint32_t image_index) = 0;
    virtual RenderTargetResult presentImage() = 0;
    virtual void removeRenderTarget(RenderTarget* render_target) = 0;
};

}  // namespace app3d::rel::d3d12
