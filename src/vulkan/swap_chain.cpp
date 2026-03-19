#include "swap_chain.h"

#include "device.h"
#include "object_destroyer.h"
#include "render_target.h"
#include "surface.h"

#include "common/logger.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// SwapChain class implementation

SwapChain::SwapChain(Device& device, Surface& surface) : device_(device), surface_(surface) {}

SwapChain::~SwapChain() {
    render_target_.reset();
    ObjectDestroyer<VkSwapchainKHR>::destroy(~device_, swap_chain_);
}

std::uint32_t SwapChain::chooseImageCount(const VkSurfaceCapabilitiesKHR& capabilities, const uxs::db::value& opts) {
    std::uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) {
        image_count = std::min<std::uint32_t>(image_count, capabilities.maxImageCount);
    }
    return image_count;
}

VkExtent2D SwapChain::chooseImageExtent(const VkSurfaceCapabilitiesKHR& capabilities, const uxs::db::value& opts) {
    VkExtent2D image_size{
        .width = opts.value<std::uint32_t>("width"),
        .height = opts.value<std::uint32_t>("height"),
    };
    if (capabilities.currentExtent.width == INVALID_UINT32_VALUE) {
        image_size.width = std::max<std::uint32_t>(
            std::min<std::uint32_t>(image_size.width, capabilities.maxImageExtent.width),
            capabilities.minImageExtent.width);
        image_size.height = std::max<std::uint32_t>(
            std::min<std::uint32_t>(image_size.height, capabilities.maxImageExtent.height),
            capabilities.minImageExtent.height);
    } else {
        image_size = capabilities.currentExtent;
    }
    return image_size;
}

bool SwapChain::chooseImageUsage(const VkSurfaceCapabilitiesKHR& capabilities, VkImageUsageFlags& usage) {
    const VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if ((capabilities.supportedUsageFlags & image_usage) != image_usage) {
        logError(LOG_VK "desired surface usage in not supported");
        return false;
    }
    usage = image_usage;
    return true;
}

VkPresentModeKHR SwapChain::choosePresentMode(std::span<const VkPresentModeKHR> present_modes) {
    const VkPresentModeKHR desired = VK_PRESENT_MODE_MAILBOX_KHR;
    if (std::ranges::any_of(present_modes, [](const auto& mode) { return mode == desired; })) { return desired; }
    logWarning(LOG_VK "MAILBOX surface present mode is not supported; selecting FIFO mode");
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkSurfaceFormatKHR SwapChain::chooseImageFormat(std::span<const VkSurfaceFormatKHR> formats) {
    VkSurfaceFormatKHR image_format = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };

    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) { return image_format; }

    if (std::ranges::any_of(formats, [&image_format](const auto& avail) {
            return avail.format == image_format.format && avail.colorSpace == image_format.colorSpace;
        })) {
        return image_format;
    }

    auto desired_format_it = std::ranges::find_if(
        formats, [&image_format](const auto& avail) { return avail.format == image_format.format; });
    if (desired_format_it != formats.end()) {
        logWarning(LOG_VK
                   "desired combination of surface format and colorspace is not supported; "
                   "selecting other colorspace");
        image_format.colorSpace = desired_format_it->colorSpace;
        return image_format;
    }

    logWarning(LOG_VK
               "desired surface format is not supported; "
               "selecting available format - colorspace combination");
    return formats[0];
}

bool SwapChain::create(const uxs::db::value& opts) {
    device_.waitDevice();
    if (!surface_.loadCapabilities(device_.getPhysicalDevice())) { return false; }

    const auto& capabilities = surface_.getCapabilities();

    const std::uint32_t image_count = chooseImageCount(capabilities, opts);
    image_extent_ = chooseImageExtent(capabilities, opts);

    if (image_extent_.width == 0 || image_extent_.height == 0) {
        logError(LOG_VK "failed to choose swap chain image size");
        return false;
    }

    if (render_target_) { render_target_->destroyImageViews(); }
    images_.clear();

    const std::uint32_t layer_count = std::max<std::uint32_t>(opts.value<std::uint32_t>("layer_count"), 1);

    const VkSwapchainCreateInfoKHR create_info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = ~surface_,
        .minImageCount = image_count,
        .imageFormat = surface_.getImageFormat().format,
        .imageColorSpace = surface_.getImageFormat().colorSpace,
        .imageExtent = image_extent_,
        .imageArrayLayers = layer_count,
        .imageUsage = surface_.getImageUsage(),
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = surface_.getPresentMode(),
        .clipped = VK_TRUE,
        .oldSwapchain = swap_chain_,
    };

    VkResult result = vkCreateSwapchainKHR(~device_, &create_info, nullptr, &swap_chain_);
    if (result != VK_SUCCESS || swap_chain_ == VK_NULL_HANDLE) {
        logError(LOG_VK "couldn't create a swap chain");
        return false;
    }

    ObjectDestroyer<VkSwapchainKHR>::destroy(~device_, create_info.oldSwapchain);

    if (!loadImageHandles()) { return false; }
    if (render_target_ && !render_target_->createImageViews()) { return false; }

    return true;
}

RenderTargetResult SwapChain::acquireImage(std::uint64_t timeout, VkSemaphore semaphore, VkFence fence,
                                           std::uint32_t& image_index) {
    VkResult result = vkAcquireNextImageKHR(~device_, swap_chain_, timeout, semaphore, fence, &image_index);
    if (result == VK_SUCCESS) { return RenderTargetResult::SUCCESS; }
    if (result == VK_SUBOPTIMAL_KHR) { return RenderTargetResult::SUBOPTIMAL; }
    if (result == VK_ERROR_OUT_OF_DATE_KHR) { return RenderTargetResult::OUT_OF_DATE; }
    return RenderTargetResult::FAILED;
}

//@{ ISwapChain

IRenderTarget* SwapChain::createRenderTarget(const uxs::db::value& opts) {
    auto render_target = std::make_unique<RenderTarget>(device_, *this);
    if (!render_target->create(opts)) { return nullptr; }
    if (!render_target->createImageViews()) { return nullptr; }
    render_target_ = std::move(render_target);
    return render_target_.get();
}

//@}

bool SwapChain::loadImageHandles() {
    std::uint32_t image_count = 0;
    VkResult result = vkGetSwapchainImagesKHR(~device_, swap_chain_, &image_count, nullptr);
    if (result != VK_SUCCESS || image_count == 0) {
        logError(LOG_VK "couldn't get the number of swap chain images");
        return false;
    }

    images_.resize(image_count, VK_NULL_HANDLE);
    result = vkGetSwapchainImagesKHR(~device_, swap_chain_, &image_count, images_.data());
    if (result != VK_SUCCESS || image_count == 0) {
        logError(LOG_VK "couldn't enumerate swap_chain images");
        return false;
    }

    return true;
}
