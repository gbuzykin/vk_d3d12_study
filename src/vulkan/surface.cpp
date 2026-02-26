#include "surface.h"

#include "device.h"
#include "rendering_driver.h"
#include "swap_chain.h"

#include "utils/logger.h"

#include <algorithm>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Surface class implementation

Surface::Surface(RenderingDriver& instance, VkSurfaceKHR surface) : instance_(instance), surface_(surface) {}

Surface::~Surface() {
    if (surface_ != VK_NULL_HANDLE) { vkDestroySurfaceKHR(instance_.getHandle(), surface_, nullptr); }
}

std::uint32_t Surface::getPresentQueueFamily(std::uint32_t n) const {
    return n < present_queue_families_.size() ? present_queue_families_[n] : INVALID_UINT32_VALUE;
}

bool Surface::obtainCapabilities(PhysicalDevice& physical_device) {
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device.getHandle(), surface_, &capabilities_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't get the capabilities of a surface");
        return false;
    }

    return true;
}

bool Surface::obtainPresentQueueFamilies(PhysicalDevice& physical_device) {
    present_queue_families_.clear();

    const auto& queue_families = physical_device.getQueueFamilies();

    for (std::uint32_t index = 0; index < std::uint32_t(queue_families.size()); ++index) {
        VkBool32 is_supported = VK_FALSE;
        VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device.getHandle(), index, surface_,
                                                               &is_supported);
        if (result == VK_SUCCESS && is_supported == VK_TRUE) { present_queue_families_.push_back(index); }
    }

    if (present_queue_families_.empty()) {
        logError(LOG_VK "couldn't obtain present queue families");
        return false;
    }

    return true;
}

bool Surface::obtainPresentModes(PhysicalDevice& physical_device) {
    std::uint32_t present_mode_count = 0;
    VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device.getHandle(), surface_,
                                                                &present_mode_count, nullptr);
    if (result != VK_SUCCESS || present_mode_count == 0) {
        logError(LOG_VK "couldn't get the number of supported present modes");
        return false;
    }

    present_modes_.resize(present_mode_count);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device.getHandle(), surface_, &present_mode_count,
                                                       present_modes_.data());
    if (result != VK_SUCCESS || present_mode_count == 0) {
        logError(LOG_VK "couldn't enumerate present modes");
        return false;
    }

    return true;
}

bool Surface::obtainFormats(PhysicalDevice& physical_device) {
    std::uint32_t formats_count = 0;
    VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device.getHandle(), surface_, &formats_count,
                                                           nullptr);
    if (result != VK_SUCCESS || formats_count == 0) {
        logError(LOG_VK "couldn't get the number of supported surface formats");
        return false;
    }

    formats_.resize(formats_count);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device.getHandle(), surface_, &formats_count,
                                                  formats_.data());
    if (result != VK_SUCCESS || formats_count == 0) {
        logError(LOG_VK "couldn't enumerate supported surface formats");
        return false;
    }

    return true;
}

std::uint32_t Surface::chooseImageCount(const SwapChainCreateInfo& create_info) {
    std::uint32_t image_count = capabilities_.minImageCount + 1;
    if (capabilities_.maxImageCount > 0) {
        image_count = std::min<std::uint32_t>(image_count, capabilities_.maxImageCount);
    }
    return image_count;
}

VkExtent2D Surface::chooseImageSize(const SwapChainCreateInfo& create_info) {
    VkExtent2D image_size = {create_info.width, create_info.height};
    if (capabilities_.currentExtent.width == INVALID_UINT32_VALUE) {
        image_size.width = std::max<std::uint32_t>(
            std::min<std::uint32_t>(image_size.width, capabilities_.maxImageExtent.width),
            capabilities_.minImageExtent.width);
        image_size.height = std::max<std::uint32_t>(
            std::min<std::uint32_t>(image_size.height, capabilities_.maxImageExtent.height),
            capabilities_.minImageExtent.height);
    } else {
        image_size = capabilities_.currentExtent;
    }
    return image_size;
}

VkSurfaceFormatKHR Surface::chooseImageFormat(const SwapChainCreateInfo& create_info) {
    VkSurfaceFormatKHR image_format = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };

    if (formats_.size() == 1 && formats_[0].format == VK_FORMAT_UNDEFINED) { return image_format; }

    if (std::ranges::any_of(formats_, [&image_format](const auto& avail) {
            return avail.format == image_format.format && avail.colorSpace == image_format.colorSpace;
        })) {
        return image_format;
    }

    auto desired_format_it = std::ranges::find_if(
        formats_, [&image_format](const auto& avail) { return avail.format == image_format.format; });
    if (desired_format_it != formats_.end()) {
        logWarning(LOG_VK
                   "desired combination of surface format and colorspace is not supported; "
                   "selecting other colorspace");
        image_format.colorSpace = desired_format_it->colorSpace;
        return image_format;
    }

    logWarning(LOG_VK
               "desired surface format is not supported; "
               "selecting available format - colorspace combination");
    return formats_[0];
}

VkPresentModeKHR Surface::choosePresentMode(const SwapChainCreateInfo& create_info) {
    const VkPresentModeKHR desired = VK_PRESENT_MODE_MAILBOX_KHR;
    if (std::ranges::any_of(present_modes_, [](const auto& mode) { return mode == desired; })) { return desired; }
    logWarning(LOG_VK "MAILBOX surface present mode is not supported; selecting FIFO mode");
    return VK_PRESENT_MODE_FIFO_KHR;
}

SwapChain* Surface::createSwapChain(Device& device, const SwapChainCreateInfo& create_info) {
    // Choose desired image usage scenarios
    const auto image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if ((capabilities_.supportedUsageFlags & image_usage) != image_usage) {
        logError(LOG_VK "desired surface usage in not supported");
        return nullptr;
    }

    const std::uint32_t image_count = chooseImageCount(create_info);
    const auto image_size = chooseImageSize(create_info);
    const auto image_format = chooseImageFormat(create_info);
    const auto present_mode = choosePresentMode(create_info);
    const auto image_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    if (image_size.width == 0 || image_size.height == 0) {
        logError(LOG_VK "failed to choose swap chain image size");
        return nullptr;
    }

    const VkSwapchainCreateInfoKHR ci{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface_,
        .minImageCount = image_count,
        .imageFormat = image_format.format,
        .imageColorSpace = image_format.colorSpace,
        .imageExtent = image_size,
        .imageArrayLayers = std::max<std::uint32_t>(create_info.layer_count, 1),
        .imageUsage = image_usage,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = image_transform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = swap_chain_ ? swap_chain_->getHandle() : VK_NULL_HANDLE,
    };

    VkSwapchainKHR swap_chain_handle;
    VkResult result = vkCreateSwapchainKHR(device.getHandle(), &ci, nullptr, &swap_chain_handle);
    if (result != VK_SUCCESS || swap_chain_handle == VK_NULL_HANDLE) {
        logError(LOG_VK "couldn't create a swap chain");
        return nullptr;
    }

    swap_chain_.reset();

    auto swap_chain = std::make_unique<SwapChain>(device, *this, swap_chain_handle);
    if (!swap_chain->obtainImages()) { return nullptr; }
    if (!swap_chain->createImageViews(image_format.format, VK_IMAGE_ASPECT_COLOR_BIT)) { return nullptr; }

    swap_chain_ = std::move(swap_chain);
    return swap_chain_.get();
}

void Surface::destroySwapChain() { swap_chain_.reset(); }
