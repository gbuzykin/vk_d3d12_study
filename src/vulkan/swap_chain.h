#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

#include <vector>

namespace app3d::rel::vulkan {

class Device;
class Surface;

class SwapChain : public ISwapChain {
 public:
    SwapChain(Device& device, Surface& surface, VkSwapchainKHR swap_chain);
    ~SwapChain() override;

    bool obtainImages();
    bool createImageViews(VkFormat format, VkImageAspectFlags aspectFlags);
    void destroyImageViews();

    VkSwapchainKHR getHandle() { return swap_chain_; }

    //@{ ISwapChain
    //@}

 private:
    Device& device_;
    Surface& surface_;
    VkSwapchainKHR swap_chain_;
    std::vector<VkImage> images_;
    std::vector<VkImageView> image_views_;
};

}  // namespace app3d::rel::vulkan
