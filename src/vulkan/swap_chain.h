#pragma once

#include "command_buffer.h"

#include <uxs/dynarray.h>

namespace app3d::rel::vulkan {

class Device;
class Surface;
class RenderTarget;

class SwapChain final : public ISwapChain {
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
    VkImage getImage(std::uint32_t image_index) { return images_[image_index]; }
    VkImageView getImageView(std::uint32_t image_index) { return image_views_[image_index]; }
    std::uint32_t getImageCount() const { return std::uint32_t(images_.size()); }
    std::uint32_t getFifCount() const { return 3; }
    void imageBarrierBefore(CommandBuffer& command_buffer, std::uint32_t image_index);
    bool imageBarrierAfter(CommandBuffer& command_buffer, std::uint32_t image_index);
    RenderTargetResult acquireImage(std::uint64_t timeout, VkSemaphore semaphore, std::uint32_t& image_index);
    RenderTargetResult presentImage(std::uint32_t n_frame, std::uint32_t image_index, VkSemaphore wait_semaphore,
                                    VkFence fence);

    VkSwapchainKHR operator~() { return swap_chain_; }
    Surface& getSurface() { return surface_; }

    //@{ ISwapChain
    Extent2u getImageExtent() const override {
        return Extent2u{.width = image_extent_.width, .height = image_extent_.height};
    }
    IRenderTarget* createRenderTarget(const uxs::db::value& opts) override;
    //@}

 private:
    Device& device_;
    Surface& surface_;
    VkSwapchainKHR swap_chain_{VK_NULL_HANDLE};
    VkExtent2D image_extent_{0, 0};
    uxs::inline_dynarray<VkImage, 3> images_;
    uxs::inline_dynarray<VkImageView, 3> image_views_;
    std::unique_ptr<RenderTarget> render_target_;

    struct PresentKit {
        VkSemaphore sem_ready_to_present{VK_NULL_HANDLE};
        CommandBuffer command_buffer;
    };

    uxs::inline_dynarray<PresentKit, 3> present_kits_;

    bool loadImageHandles();
    bool createImageViews();
    void destroyImageViews();
};

}  // namespace app3d::rel::vulkan
