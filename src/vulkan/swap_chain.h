#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"
#include "util/range_helpers.h"

#include <vector>

namespace app3d::rel::vulkan {

class Device;
class Surface;

class SwapChain : public ISwapChain {
 public:
    SwapChain(Device& device, Surface& surface);
    ~SwapChain() override;

    std::uint32_t getImageCount() const { return image_count_; }
    std::uint32_t getImageWidth() const { return image_size_.width; }
    std::uint32_t getImageHeight() const { return image_size_.height; }
    VkExtent2D getImageSize() const { return image_size_; }
    VkRect2D getImageRect() const { return {.offset = {0, 0}, .extent = image_size_}; }
    VkImage getImage(std::uint32_t image_index) const { return images_[image_index]; }
    VkImageView getImageView(std::uint32_t image_index) const { return image_views_[image_index]; }

    static std::uint32_t chooseImageCount(const VkSurfaceCapabilitiesKHR& capabilities, const uxs::db::value& opts);
    static VkExtent2D chooseImageSize(const VkSurfaceCapabilitiesKHR& capabilities, const uxs::db::value& opts);
    static bool chooseImageUsage(const VkSurfaceCapabilitiesKHR& capabilities, VkImageUsageFlags& usage);
    static VkPresentModeKHR choosePresentMode(std::span<const VkPresentModeKHR> present_modes);
    static VkSurfaceFormatKHR chooseImageFormat(std::span<const VkSurfaceFormatKHR> formats);

    bool create(const uxs::db::value& opts);
    RenderTargetResult acquireImage(std::uint64_t timeout, VkSemaphore semaphore, VkFence fence,
                                    std::uint32_t& image_index);

    VkSwapchainKHR operator~() { return swap_chain_; }
    Surface& getSurface() { return surface_; }

    //@{ ISwapChain
    //@}

 private:
    Device& device_;
    Surface& surface_;
    VkSwapchainKHR swap_chain_{VK_NULL_HANDLE};
    std::uint32_t image_count_ = 0;
    VkExtent2D image_size_{0, 0};
    std::vector<VkImage> images_;
    std::vector<VkImageView> image_views_;

    bool loadImageHandles();
    bool createImageViews(VkFormat format, VkImageAspectFlags aspectFlags);
    void destroyImageViews();
};

}  // namespace app3d::rel::vulkan
