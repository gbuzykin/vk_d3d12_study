#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

#include <vector>

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

    std::uint32_t getImageCount() const { return std::uint32_t(images_.size()); }
    VkImage getImage(std::uint32_t image_index) const { return images_[image_index]; }

    static std::uint32_t chooseImageCount(const VkSurfaceCapabilitiesKHR& capabilities,
                                          const uxs::db::value& create_info);
    static VkExtent2D chooseImageExtent(const VkSurfaceCapabilitiesKHR& capabilities, const uxs::db::value& create_info);
    static bool chooseImageUsage(const VkSurfaceCapabilitiesKHR& capabilities, VkImageUsageFlags& usage);
    static VkPresentModeKHR choosePresentMode(std::span<const VkPresentModeKHR> present_modes);
    static VkSurfaceFormatKHR chooseImageFormat(std::span<const VkSurfaceFormatKHR> formats);

    bool create(const uxs::db::value& create_info);
    ResultCode acquireImage(std::uint64_t timeout, VkSemaphore semaphore, VkFence fence, std::uint32_t& image_index);

    VkSwapchainKHR operator~() { return swap_chain_; }
    Surface& getSurface() { return surface_; }

    //@{ ISwapChain
    Extent2u getImageExtent() const override {
        return Extent2u{.width = image_extent_.width, .height = image_extent_.height};
    }
    IRenderTarget* createRenderTarget(const uxs::db::value& create_info) override;
    //@}

 private:
    Device& device_;
    Surface& surface_;
    VkSwapchainKHR swap_chain_{VK_NULL_HANDLE};
    VkExtent2D image_extent_{0, 0};
    std::vector<VkImage> images_;
    std::unique_ptr<RenderTarget> render_target_;

    bool loadImageHandles();
};

}  // namespace app3d::rel::vulkan
