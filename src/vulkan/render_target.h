#pragma once

#include "command_buffer.h"

#include <uxs/dynarray.h>

namespace app3d::rel::vulkan {

class Device;
class ImageProvider;
class Pipeline;

class RenderTarget final : public util::ref_counter, public IRenderTarget {
 public:
    RenderTarget(Device& device, ImageProvider& image_provider);
    ~RenderTarget() override;

    bool useDepth() const { return use_depth_; }

    bool create(const uxs::db::value& opts);
    bool createFrameResources();
    void destroyFrameResources();

    VkRenderPass getRenderPass() { return render_pass_; }

    //@{ IUnknown
    util::ref_counter& getRefCounter() override { return *this; }
    //@}

    //@{ IRenderTarget
    Extent2u getImageExtent() const override { return {.width = image_extent_.width, .height = image_extent_.height}; }
    std::uint32_t getFifCount() const override { return std::uint32_t(frame_render_kits_.size()); }
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
    util::reference<Device> device_;
    util::reference<ImageProvider> image_provider_;
    bool use_depth_ = false;
    VkExtent2D image_extent_{};
    VkFormat depth_format_{VK_FORMAT_D16_UNORM};
    VkRenderPass render_pass_{VK_NULL_HANDLE};
    RenderTargetResult render_target_status_{RenderTargetResult::SUCCESS};
    Pipeline* current_pipeline_ = nullptr;

    struct FrameResources {
        VkImage depth_image{VK_NULL_HANDLE};
        VmaAllocation depth_allocation{VK_NULL_HANDLE};
        VkImageView depth_image_view{VK_NULL_HANDLE};
        VkFramebuffer framebuffer{VK_NULL_HANDLE};
    };

    std::uint32_t current_image_index_ = INVALID_UINT32_VALUE;
    uxs::inline_dynarray<FrameResources, 3> frame_resources_;

    struct FrameRenderKit {
        VkSemaphore sem_image_acquired{VK_NULL_HANDLE};
        VkSemaphore sem_rendering_complete{VK_NULL_HANDLE};
        VkFence fence{VK_NULL_HANDLE};
        CommandBuffer command_buffer;
    };

    std::uint32_t n_frame_ = 0;
    uxs::inline_dynarray<FrameRenderKit, 3> frame_render_kits_;
};

}  // namespace app3d::rel::vulkan
