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

    VkRenderPass getRenderPass() { return render_pass_; }

    //@{ IRenderTarget
    RenderTargetResult beginRenderTarget(const Color4f& clear_color) override;
    bool endRenderTarget() override;
    void setViewport(const Rect& rect, float z_near, float z_far) override;
    void setScissor(const Rect& rect) override;
    void bindPipeline(IPipeline& pipeline) override;
    void bindVertexBuffer(IBuffer& buffer, std::size_t offset, std::uint32_t binding) override;
    void bindDescriptorSet(IPipeline& pipeline, IDescriptorSet& descriptor_set, std::uint32_t set_index) override;
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
    VkSemaphore sem_ready_to_present_{VK_NULL_HANDLE};
    VkFence fence_drawing_{VK_NULL_HANDLE};
    VkRenderPass render_pass_{VK_NULL_HANDLE};
    std::vector<ImageViewData> image_views_;
    RenderTargetResult swap_chain_status_ = RenderTargetResult::SUCCESS;
    std::uint32_t current_image_index_ = INVALID_UINT32_VALUE;
};

}  // namespace app3d::rel::vulkan
