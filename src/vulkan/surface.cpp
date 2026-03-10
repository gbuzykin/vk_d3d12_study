#include "surface.h"

#include "device.h"
#include "object_destroyer.h"
#include "rendering_driver.h"
#include "swap_chain.h"

#include "utils/logger.h"

#include <algorithm>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

namespace {
struct WindowDescImpl {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    HINSTANCE hinstance;
    HWND hwnd;
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    Display* dpy;
    Window window;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    xcb_connection_t* connection;
    xcb_window_t window;
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    wl_display* display;
    wl_surface* surface;
#endif
};
static_assert(sizeof(WindowHandle) >= sizeof(WindowDescImpl), "Too little WindowHandle size");
static_assert(std::alignment_of_v<WindowHandle> >= std::alignment_of_v<WindowDescImpl>,
              "Too little WindowHandle alignment");
}  // namespace

// --------------------------------------------------------
// Surface class implementation

Surface::Surface(RenderingDriver& instance) : instance_(instance) {}

Surface::~Surface() { ObjectDestroyer<VkSurfaceKHR>::destroy(~instance_, surface_); }

std::uint32_t Surface::getPresentQueueFamily(std::uint32_t n) const {
    return n < present_queue_families_.size() ? present_queue_families_[n] : INVALID_UINT32_VALUE;
}

bool Surface::create(const WindowHandle& window_handle) {
    const WindowDescImpl& win_desc_impl = *reinterpret_cast<const WindowDescImpl*>(&window_handle.v);

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    const VkWin32SurfaceCreateInfoKHR create_info{
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = win_desc_impl.hinstance,
        .hwnd = win_desc_impl.hwnd,
    };

    VkResult result = vkCreateWin32SurfaceKHR(~instance_, &create_info, nullptr, &surface_);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    const VkXlibSurfaceCreateInfoKHR create_info{
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .dpy = win_desc_impl.dpy,
        .window = win_desc_impl.window,
    };

    VkResult result = vkCreateXlibSurfaceKHR(~instance_, &create_info, nullptr, &surface_);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    const VkXcbSurfaceCreateInfoKHR create_info{
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .connection = win_desc_impl.connection,
        .window = win_desc_impl.window,
    };

    VkResult result = vkCreateXcbSurfaceKHR(~instance_, &create_info, nullptr, &surface_);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    const VkWaylandSurfaceCreateInfoKHR create_info{
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .display = win_desc_impl.display,
        .surface = win_desc_impl.surface,
    };

    VkResult result = vkCreateWaylandSurfaceKHR(~instance_, &create_info, nullptr, &surface_);
#endif

    if (result != VK_SUCCESS || surface_ == VK_NULL_HANDLE) {
        logError(LOG_VK "couldn't create surface");
        return false;
    }

    return true;
}

bool Surface::loadCapabilities(PhysicalDevice& physical_device) {
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(~physical_device, surface_, &capabilities_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't get the capabilities of a surface");
        return false;
    }

    return true;
}

bool Surface::loadFormats(PhysicalDevice& physical_device) {
    std::uint32_t formats_count = 0;
    VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(~physical_device, surface_, &formats_count, nullptr);
    if (result != VK_SUCCESS || formats_count == 0) {
        logError(LOG_VK "couldn't get the number of supported surface formats");
        return false;
    }

    formats_.resize(formats_count);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(~physical_device, surface_, &formats_count, formats_.data());
    if (result != VK_SUCCESS || formats_count == 0) {
        logError(LOG_VK "couldn't enumerate supported surface formats");
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
        logError(LOG_VK "couldn't get the number of supported present modes");
        return false;
    }

    present_modes_.resize(present_mode_count);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(~physical_device, surface_, &present_mode_count,
                                                       present_modes_.data());
    if (result != VK_SUCCESS || present_mode_count == 0) {
        logError(LOG_VK "couldn't enumerate present modes");
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

void Surface::destroySwapChain() { swap_chain_.reset(); }

//@{ ISurface

ISwapChain* Surface::createSwapChain(IDevice& device, const uxs::db::value& create_info) {
    if (!swap_chain_) {
        auto swap_chain = std::make_unique<SwapChain>(static_cast<Device&>(device), *this);
        if (!swap_chain->create(create_info)) { return nullptr; }
        swap_chain_ = std::move(swap_chain);
        return swap_chain_.get();
    } else if (!swap_chain_->create(create_info)) {
        return nullptr;
    }
    return swap_chain_.get();
}

//@}
