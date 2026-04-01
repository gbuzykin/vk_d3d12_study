#pragma once

#include "command_buffer.h"

#include <uxs/dynarray.h>

namespace app3d::rel::vulkan {

class Device;
class SwapChain;
class Pipeline;

class RenderTarget final : public IRenderTarget {
 public:
    RenderTarget(Device& device, SwapChain& swap_chain);
    ~RenderTarget() override;
    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    bool useDepth() const { return use_depth_; }

    bool create(const uxs::db::value& opts);
    bool createFrameResources();
    void destroyFrameResources();

    VkRenderPass getRenderPass() { return render_pass_; }

    //@{ IRenderTarget
    std::uint32_t getFrameInFlightCount() const override { return std::uint32_t(frame_render_kits_.size()); }
    RenderTargetResult beginRenderTarget(const Color4f& clear_color, float depth, std::uint32_t stencil) override;
    bool endRenderTarget() override;
    void setViewport(const Rect& rect, float z_near, float z_far) override;
    void setScissor(const Rect& rect) override;
    void bindPipeline(IPipeline& pipeline) override;
    void bindVertexBuffer(IBuffer& buffer, std::size_t offset, std::uint32_t slot) override;
    void bindDescriptorSet(IDescriptorSet& descriptor_set, std::uint32_t set_index) override;
    void setPrimitiveTopology(PrimitiveTopology topology) override;
    void drawGeometry(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex,
                      std::uint32_t first_instance) override;
    //@}

 private:
    struct FrameResources {
        VkImage depth_image{VK_NULL_HANDLE};
        VmaAllocation depth_allocation{VK_NULL_HANDLE};
        VkImageView depth_image_view{VK_NULL_HANDLE};
        VkFramebuffer framebuffer{VK_NULL_HANDLE};
    };

    struct FrameRenderKit {
        VkSemaphore sem_image_acquired{VK_NULL_HANDLE};
        VkSemaphore sem_ready_to_present{VK_NULL_HANDLE};
        VkFence fence_drawing{VK_NULL_HANDLE};
        CommandBuffer command_buffer;
    };

    Device& device_;
    SwapChain& swap_chain_;
    bool use_depth_ = false;
    VkFormat depth_format_ = VK_FORMAT_D16_UNORM;
    VkRenderPass render_pass_{VK_NULL_HANDLE};
    RenderTargetResult swap_chain_status_ = RenderTargetResult::SUCCESS;
    std::uint32_t current_image_index_ = INVALID_UINT32_VALUE;
    Pipeline* current_pipeline_ = nullptr;
    uxs::inline_dynarray<FrameResources, 3> frame_resources_;

    static constexpr unsigned MAX_FIF_COUNT = 3;
    unsigned n_frame_ = 0;
    uxs::inline_dynarray<FrameRenderKit, MAX_FIF_COUNT> frame_render_kits_;
};

}  // namespace app3d::rel::vulkan
