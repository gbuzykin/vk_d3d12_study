#include "surface.h"

#include "device.h"
#include "object_destroyer.h"
#include "rendering_driver.h"
#include "swap_chain.h"
#include "vulkan_logger.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Surface class implementation

Surface::Surface(RenderingDriver& instance) : instance_(instance) {}

Surface::~Surface() { ObjectDestroyer<VkSurfaceKHR>::destroy(~instance_, surface_); }

std::uint32_t Surface::getPresentQueueFamily(std::uint32_t n) const {
    return n < present_queue_families_.size() ? present_queue_families_[n] : INVALID_UINT32_VALUE;
}

bool Surface::create(const WindowDescriptor& win_desc) {
    VkResult result = VK_SUCCESS;

    switch (win_desc.platform) {
#ifdef VK_USE_PLATFORM_WIN32_KHR
        case PlatformType::PLATFORM_WIN32: {
            struct WindowDescImpl {
                HINSTANCE hinstance;
                HWND hwnd;
            };
            static_assert(sizeof(win_desc.handle) >= sizeof(WindowDescImpl), "Too little WindowDescriptor size");
            static_assert(std::alignment_of_v<decltype(win_desc.handle)> >= std::alignment_of_v<WindowDescImpl>,
                          "Too little WindowDescriptor alignment");
            const WindowDescImpl& win_desc_impl = *reinterpret_cast<const WindowDescImpl*>(&win_desc.handle);
            const VkWin32SurfaceCreateInfoKHR create_info{
                .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                .hinstance = win_desc_impl.hinstance,
                .hwnd = win_desc_impl.hwnd,
            };
            result = vkCreateWin32SurfaceKHR(~instance_, &create_info, nullptr, &surface_);
        } break;
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
        case PlatformType::PLATFORM_XLIB: {
            struct WindowDescImpl {
                Display* dpy;
                Window window;
            };
            static_assert(sizeof(win_desc.handle) >= sizeof(WindowDescImpl), "Too little WindowDescriptor size");
            static_assert(std::alignment_of_v<decltype(win_desc.handle)> >= std::alignment_of_v<WindowDescImpl>,
                          "Too little WindowDescriptor alignment");
            const WindowDescImpl& win_desc_impl = *reinterpret_cast<const WindowDescImpl*>(&win_desc.handle);
            const VkXlibSurfaceCreateInfoKHR create_info{
                .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
                .dpy = win_desc_impl.dpy,
                .window = win_desc_impl.window,
            };
            result = vkCreateXlibSurfaceKHR(~instance_, &create_info, nullptr, &surface_);
        } break;
#endif
#ifdef VK_USE_PLATFORM_XCB_KHR
        case PlatformType::PLATFORM_XCB: {
            struct WindowDescImpl {
                xcb_connection_t* connection;
                xcb_window_t window;
            };
            static_assert(sizeof(win_desc.handle) >= sizeof(WindowDescImpl), "Too little WindowDescriptor size");
            static_assert(std::alignment_of_v<decltype(win_desc.handle)> >= std::alignment_of_v<WindowDescImpl>,
                          "Too little WindowDescriptor alignment");
            const WindowDescImpl& win_desc_impl = *reinterpret_cast<const WindowDescImpl*>(&win_desc.handle);
            const VkXcbSurfaceCreateInfoKHR create_info{
                .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
                .connection = win_desc_impl.connection,
                .window = win_desc_impl.window,
            };
            result = vkCreateXcbSurfaceKHR(~instance_, &create_info, nullptr, &surface_);
        } break;
#endif
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
        case PlatformType::PLATFORM_WAYLAND: {
            struct WindowDescImpl {
                wl_display* display;
                wl_surface* surface;
            };
            static_assert(sizeof(win_desc.handle) >= sizeof(WindowDescImpl), "Too little WindowDescriptor size");
            static_assert(std::alignment_of_v<decltype(win_desc.handle)> >= std::alignment_of_v<WindowDescImpl>,
                          "Too little WindowDescriptor alignment");
            const WindowDescImpl& win_desc_impl = *reinterpret_cast<const WindowDescImpl*>(&win_desc.handle);
            const VkWaylandSurfaceCreateInfoKHR create_info{
                .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
                .display = win_desc_impl.display,
                .surface = win_desc_impl.surface,
            };
            result = vkCreateWaylandSurfaceKHR(~instance_, &create_info, nullptr, &surface_);
        } break;
#endif
        default: {
            logError(LOG_VK "unsupported platform");
            return false;
        } break;
    }

    if (result != VK_SUCCESS || surface_ == VK_NULL_HANDLE) {
        logError(LOG_VK "couldn't create surface: {}", result);
        return false;
    }

    return true;
}

bool Surface::loadCapabilities(PhysicalDevice& physical_device) {
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(~physical_device, surface_, &capabilities_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't get surface capabilities: {}", result);
        return false;
    }

    return true;
}

bool Surface::loadFormats(PhysicalDevice& physical_device) {
    std::uint32_t formats_count = 0;
    VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(~physical_device, surface_, &formats_count, nullptr);
    if (result != VK_SUCCESS || formats_count == 0) {
        logError(LOG_VK "couldn't get the number of supported surface formats: {}", result);
        return false;
    }

    formats_.resize(formats_count);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(~physical_device, surface_, &formats_count, formats_.data());
    if (result != VK_SUCCESS || formats_count == 0) {
        logError(LOG_VK "couldn't enumerate supported surface formats: {}", result);
        return false;
    }

    return true;
}

bool Surface::loadPresentQueueFamilies(PhysicalDevice& physical_device) {
    present_queue_families_.clear();

    const auto& queue_families = physical_device.getQueueFamilies();

    for (std::uint32_t index = 0; index < std::uint32_t(queue_families.size()); ++index) {
        VkBool32 is_supported = VK_FALSE;
        VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(~physical_device, index, surface_, &is_supported);
        if (result == VK_SUCCESS && is_supported == VK_TRUE) { present_queue_families_.push_back(index); }
    }

    if (present_queue_families_.empty()) {
        logError(LOG_VK "couldn't obtain present queue families");
        return false;
    }

    return true;
}

bool Surface::loadPresentModes(PhysicalDevice& physical_device) {
    std::uint32_t present_mode_count = 0;
    VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(~physical_device, surface_, &present_mode_count,
                                                                nullptr);
    if (result != VK_SUCCESS || present_mode_count == 0) {
        logError(LOG_VK "couldn't get the number of supported present modes: {}", result);
        return false;
    }

    present_modes_.resize(present_mode_count);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(~physical_device, surface_, &present_mode_count,
                                                       present_modes_.data());
    if (result != VK_SUCCESS || present_mode_count == 0) {
        logError(LOG_VK "couldn't enumerate present modes: {}", result);
        return false;
    }

    return true;
}

bool Surface::checkAndSelectSurfaceFeatures() {
    if (!SwapChain::chooseImageUsage(capabilities_, image_usage_)) { return false; }
    selected_format_ = SwapChain::chooseImageFormat(formats_);
    selected_present_mode_ = SwapChain::choosePresentMode(present_modes_);
    return true;
}

//@{ ISurface

//@}
