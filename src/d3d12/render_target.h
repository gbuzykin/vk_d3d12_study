#pragma once

#include "fence.h"

#include "common/core_defs.h"
#include "interfaces/i_rendering_driver.h"

#include <uxs/dynarray.h>

namespace app3d::rel::d3d12 {

class Device;
class FrameImageProvider;
class Pipeline;
class PipelineLayout;

class RenderTarget final : public util::ref_counter, public IRenderTarget {
 public:
    RenderTarget(Device& device, FrameImageProvider& image_provider);
    ~RenderTarget() override;

    bool useDepth() const { return use_depth_; }
    DXGI_FORMAT getImageFormat() const { return image_format_; }
    DXGI_FORMAT getDepthStencilFormat() const { return depth_stencil_format_; }

    bool create(const uxs::db::value& opts);
    bool createFrameResources();
    void destroyFrameResources();

    //@{ IRenderTarget
    util::ref_counter& getRefCounter() override { return *this; }
    Extent2u getImageExtent() const override { return image_extent_; }
    std::uint32_t getFifCount() const override { return std::uint32_t(frame_render_kits_.size()); }
    bool isInvertedNdcY() const override { return false; }
    RenderTargetResult beginRenderTarget(const Color4f& clear_color, float depth, std::uint32_t stencil,
                                         IPipeline& pipeline) override;
    bool endRenderTarget() override;
    void setViewport(const Rect& rect, float z_near, float z_far) override;
    void setScissor(const Rect& rect) override;
    void bindPipeline(IPipeline& pipeline) override;
    void bindVertexBuffer(IBuffer& buffer, std::uint32_t slot, std::uint32_t stride, std::uint32_t offset) override;
    void bindDescriptorSet(IDescriptorSet& descriptor_set, std::uint32_t set_index) override;
    void bindDescriptorSetDynamic(IDescriptorSet& descriptor_set, std::uint32_t set_index,
                                  std::span<const std::uint32_t> offsets) override;
    void setPrimitiveTopology(PrimitiveTopology topology) override;
    void drawGeometry(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex,
                      std::uint32_t first_instance) override;
    //@}

 private:
    util::ref_ptr<Device> device_;
    util::ref_ptr<FrameImageProvider> frame_image_provider_;
    bool use_depth_ = false;
    Extent2u image_extent_{};
    DXGI_FORMAT image_format_{};
    DXGI_FORMAT depth_stencil_format_{DXGI_FORMAT_D24_UNORM_S8_UINT};
    RenderTargetResult render_target_status_{RenderTargetResult::SUCCESS};
    Pipeline* current_pipeline_ = nullptr;
    util::ref_ptr<ID3D12DescriptorHeap> rtv_heap_;
    util::ref_ptr<ID3D12DescriptorHeap> dsv_heap_;

    struct FrameRenderKit {
        Fence fence;
        util::ref_ptr<D3D12MA::Allocation> depth_stencil_allocation;
        util::ref_ptr<ID3D12GraphicsCommandList> command_list;
    };

    std::uint32_t n_frame_ = 0;
    std::uint32_t image_count_ = 0;
    std::uint32_t current_image_index_ = INVALID_UINT32_VALUE;
    uxs::inline_dynarray<FrameRenderKit, 3> frame_render_kits_;
};

}  // namespace app3d::rel::d3d12
