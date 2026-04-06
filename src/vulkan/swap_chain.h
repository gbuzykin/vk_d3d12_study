#pragma once

#include "command_buffer.h"
#include "frame_image_provider.h"

#include <uxs/dynarray.h>

namespace app3d::rel::vulkan {

class Device;
class Surface;
class RenderTarget;
class CommandBuffer;

class SwapChain final : public FrameImageProvider, public ISwapChain {
 public:
    SwapChain(Device& device, Surface& surface);
    ~SwapChain() override;
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;

    static std::uint32_t chooseImageCount(const VkSurfaceCapabilitiesKHR& capabilities, const uxs::db::value& opts);
    static VkExtent2D chooseImageExtent(const VkSurfaceCapabilitiesKHR& capabilities, const uxs::db::value& opts);
    static bool chooseImageUsage(const VkSurfaceCapabilitiesKHR& capabilities, VkImageUsageFlags& usage);
    static VkPresentModeKHR choosePresentMode(std::span<const VkPresentModeKHR> present_modes);
    static VkSurfaceFormatKHR chooseImageFormat(std::span<const VkSurfaceFormatKHR> formats);

    bool create(const uxs::db::value& opts);

    VkSwapchainKHR operator~() { return swap_chain_; }

    //@{ FrameImageProvider
    VkImageView getImageView(std::uint32_t image_index) override { return image_views_[image_index]; }
    std::uint32_t getImageCount() const override { return std::uint32_t(images_.size()); }
    std::uint32_t getFifCount() const override { return 3; }
    VkFormat getImageFormat() const override;
    VkExtent2D getImageExtent() const override { return image_extent_; }
    VkPipelineStageFlags getImageConsumingStages() const override;
    VkAccessFlags getImageAccess() const override;
    VkImageLayout getImageLayout() const override;
    void imageBarrierBefore(CommandBuffer& command_buffer, std::uint32_t image_index) override;
    void imageBarrierAfter(CommandBuffer& command_buffer, std::uint32_t image_index) override;
    RenderTargetResult acquireFrameImage(std::uint32_t n_frame, std::uint64_t timeout,
                                         std::uint32_t& image_index) override;
    RenderTargetResult submitFrameImage(std::uint32_t n_frame, std::uint32_t image_index, CommandBuffer& command_buffer,
                                        VkFence fence) override;
    //@}

    //@{ ISwapChain
    bool recreate(const uxs::db::value& opts) override;
    IRenderTarget* createRenderTarget(const uxs::db::value& opts) override;
    //@}

 private:
    Device& device_;
    Surface& surface_;
    VkSwapchainKHR swap_chain_{VK_NULL_HANDLE};
    VkExtent2D image_extent_{};
    uxs::inline_dynarray<VkImage, 3> images_;
    uxs::inline_dynarray<VkImageView, 3> image_views_;
    RenderTarget* render_target_ = nullptr;

    struct SubmitImageKit {
        VkSemaphore sem_image_acquired{VK_NULL_HANDLE};
        VkSemaphore sem_rendering_complete{VK_NULL_HANDLE};
        VkSemaphore sem_ready_to_present{VK_NULL_HANDLE};
        CommandBuffer present_command_buffer;
    };

    uxs::inline_dynarray<SubmitImageKit, 3> submit_kits_;

    bool loadImageHandles();
    bool createImageViews();
    void destroyImageViews();
};

}  // namespace app3d::rel::vulkan
