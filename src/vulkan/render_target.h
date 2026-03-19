#pragma once

#include "command_buffer.h"

#include <vector>

namespace app3d::rel::vulkan {

class Device;
class SwapChain;

class RenderTarget final : public IRenderTarget {
 public:
    RenderTarget(Device& device, SwapChain& swap_chain);
    ~RenderTarget() override;
    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    bool create(const uxs::db::value& opts);
    bool createImageViews();
    void destroyImageViews();

    VkRenderPass getRenderPassHandle() { return render_pass_; }

    //@{ IRenderTarget
    RenderTargetResult beginRenderTarget(const Color4f& clear_color) override;
    bool endRenderTarget() override;
    void setViewport(const Rect& rect, float z_near, float z_far) override;
    void setScissor(const Rect& rect) override;
    void bindPipeline(IPipeline& pipeline) override;
    void bindVertexBuffer(IBuffer& buffer, std::uint32_t offset, std::uint32_t slot) override;
    void drawGeometry(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex,
                      std::uint32_t first_instance) override;
    //@}

 private:
    struct ImageViewData {
        VkImageView image_view{VK_NULL_HANDLE};
        VkFramebuffer framebuffer{VK_NULL_HANDLE};
    };

    Device& device_;
    SwapChain& swap_chain_;
    CommandBuffer command_buffer_;
    VkSemaphore sem_image_acquired_{VK_NULL_HANDLE};
    VkFence fence_drawing_{VK_NULL_HANDLE};
    VkRenderPass render_pass_{VK_NULL_HANDLE};
    std::vector<ImageViewData> image_views_;
    RenderTargetResult render_target_status_ = RenderTargetResult::SUCCESS;
    std::uint32_t current_image_index_ = INVALID_UINT32_VALUE;

    static constexpr std::uint64_t FINISH_FRAME_TIMEOUT = 5'000'000'000;
    static constexpr std::uint64_t ACQUIRE_FRAME_IMAGE_TIMEOUT = 2'000'000'000;

    std::uint32_t n_frame_ = 0;
    std::vector<VkSemaphore> sem_ready_to_present_;
};

}  // namespace app3d::rel::vulkan
