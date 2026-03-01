#include "swap_chain.h"

#include "device.h"
#include "rendering_driver.h"

#include "utils/logger.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// SwapChain class implementation

SwapChain::SwapChain(Device& device, Surface& surface, VkSwapchainKHR swap_chain)
    : device_(device), surface_(surface), swap_chain_(swap_chain) {}

SwapChain::~SwapChain() {
    destroyImageViews();
    if (swap_chain_ != VK_NULL_HANDLE) { vkDestroySwapchainKHR(device_.getHandle(), swap_chain_, nullptr); }
}

bool SwapChain::obtainImages() {
    std::uint32_t image_count = 0;
    VkResult result = vkGetSwapchainImagesKHR(device_.getHandle(), swap_chain_, &image_count, nullptr);
    if (result != VK_SUCCESS || image_count == 0) {
        logError(LOG_VK "couldn't get the number of swap chain images");
        return false;
    }

    images_.resize(image_count);
    result = vkGetSwapchainImagesKHR(device_.getHandle(), swap_chain_, &image_count, images_.data());
    if (result != VK_SUCCESS || image_count == 0) {
        logError(LOG_VK "couldn't enumerate swap_chain images");
        return false;
    }

    return true;
}

bool SwapChain::createImageViews(VkFormat format, VkImageAspectFlags aspectFlags) {
    image_views_.resize(images_.size());

    for (std::uint32_t n = 0; n < std::uint32_t(images_.size()); ++n) {
        const VkImageViewCreateInfo ci{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = images_[n],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange =
                {
                    .aspectMask = aspectFlags,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };

        VkResult result = vkCreateImageView(device_.getHandle(), &ci, nullptr, &image_views_[n]);
        if (result != VK_SUCCESS) {
            logError(LOG_VK "couldn't create image view for a swap chain image");
            return false;
        }
    }

    return true;
}

void SwapChain::destroyImageViews() {
    for (const auto& image_view : image_views_) { vkDestroyImageView(device_.getHandle(), image_view, nullptr); }
}

//@{ ISwapChain

bool SwapChain::acquireImage(std::uint64_t timeout, std::uint32_t& image_index, SemaphoreHandle* semaphore,
                             FenceHandle* fence) {
    return false;
}

bool SwapChain::queuePresent(std::uint64_t timeout, std::span<const FenceHandle> semaphores,
                             std::span<const PresentImageInfo> images) {
    return false;
}

//@}
