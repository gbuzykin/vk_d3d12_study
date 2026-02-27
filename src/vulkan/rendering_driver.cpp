#include "rendering_driver.h"

#include "device.h"
#include "surface.h"

#include "utils/dynamic_library.h"
#include "utils/print.h"

#include <algorithm>
#include <iterator>
#include <utility>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

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

static_assert(sizeof(WindowDescriptor) >= sizeof(WindowDescImpl), "Too little WindowDescriptor size");
static_assert(std::alignment_of_v<WindowDescriptor> >= std::alignment_of_v<WindowDescImpl>,
              "Too little WindowDescriptor alignment");

// --------------------------------------------------------
// RenderingDriver class implementation

RenderingDriver::RenderingDriver() {}

RenderingDriver::~RenderingDriver() {
    device_.reset();

    if (instance_) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

    if (vulkan_library_) {
        freeDynamicLibrary(vulkan_library_);
        vulkan_library_ = nullptr;
    }
}

bool RenderingDriver::isExtensionSupported(const char* extension) const {
    return std::ranges::any_of(extensions_, [extension](const auto& item) {
        return std::string_view(extension) == std::string_view(item.extensionName);
    });
}

bool RenderingDriver::init(const ApplicationInfo& app_info) {
    if (!loadVulkanLoaderLibrary()) { return false; }
    if (!obtainExtensionProperties()) { return false; }

    InstanceCreateInfo create_info;

    create_info.extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#endif
    };

    if (!createInstance(app_info, create_info)) { return false; }

    if (!obtainPhysicalDeviceList()) { return false; }

    return true;
}

std::uint32_t RenderingDriver::getPhysicalDeviceCount() const {
    return static_cast<std::uint32_t>(physical_devices_.size());
}

const char* RenderingDriver::getPhysicalDeviceName(std::uint32_t device_index) const {
    if (device_index >= static_cast<std::uint32_t>(physical_devices_.size())) {
        logError("invalid physical device index");
        return nullptr;
    }

    return physical_devices_[device_index]->getName();
}

bool RenderingDriver::isSuitablePhysicalDevice(std::uint32_t device_index, const DesiredDeviceCaps& caps) const {
    if (device_index >= static_cast<std::uint32_t>(physical_devices_.size())) {
        logError("invalid physical device index");
        return false;
    }

    return physical_devices_[device_index]->isSuitableDevice(caps);
}

ISurface* RenderingDriver::createSurface(const WindowDescriptor& window_desc) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    const WindowDescImpl& win_desc_impl = *reinterpret_cast<const WindowDescImpl*>(&window_desc.v);

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    VkWin32SurfaceCreateInfoKHR surface_create_info{
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = win_desc_impl.hinstance,
        .hwnd = win_desc_impl.hwnd,
    };

    VkResult result = vkCreateWin32SurfaceKHR(instance_, &surface_create_info, nullptr, &surface);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    VkXlibSurfaceCreateInfoKHR surface_create_info{
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .dpy = win_desc_impl.dpy,
        .window = win_desc_impl.window,
    };

    VkResult result = vkCreateXlibSurfaceKHR(instance_, &surface_create_info, nullptr, &surface);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    VkXcbSurfaceCreateInfoKHR surface_create_info{
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .connection = win_desc_impl.connection,
        .window = win_desc_impl.window,
    };

    VkResult result = vkCreateXcbSurfaceKHR(instance_, &surface_create_info, nullptr, &surface);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    VkWaylandSurfaceCreateInfoKHR surface_create_info{
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .display = win_desc_impl.display,
        .surface = win_desc_impl.surface,
    };

    VkResult result = vkCreateWaylandSurfaceKHR(instance_, &surface_create_info, nullptr, &surface);
#endif

    if (result != VK_SUCCESS || surface == VK_NULL_HANDLE) {
        printError("couldn't create surface");
        return nullptr;
    }

    // if (!surface->obtainCapabilities()) { return nullptr; }
    // if (!surface->obtainPresentQueueFamilies()) { return nullptr; }
    // if (!surface->obtainPresentModes()) { return nullptr; }

    surface_ = std::make_unique<Surface>(*this, surface);
    return surface_.get();
}

IDevice* RenderingDriver::creatDevice(std::uint32_t device_index, ISurface& surface, const DesiredDeviceCaps& caps) {
    if (device_index >= static_cast<std::uint32_t>(physical_devices_.size())) {
        logError("invalid physical device index");
        return nullptr;
    }

    auto logical_device = std::make_unique<Device>(*physical_devices_[device_index]);
    if (!logical_device->create(static_cast<Surface&>(surface), caps)) { return nullptr; }

    device_ = std::move(logical_device);
    return device_.get();
}

bool RenderingDriver::loadVulkanLoaderLibrary() {
#if defined(WIN32)
    vulkan_library_ = loadDynamicLibrary("", "vulkan-1");
#elif defined(__linux__)
    vulkan_library_ = loadDynamicLibrary("", "vulkan");
#endif
    if (!vulkan_library_) { return false; }

#define EXPORTED_VK_FUNCTION(name) \
    name = (PFN_##name)getDynamicLibraryEntry(vulkan_library_, #name); \
    if (!name) { return false; }

#define GLOBAL_LEVEL_VK_FUNCTION(name) \
    name = (PFN_##name)vkGetInstanceProcAddr(nullptr, #name); \
    if (!name) { \
        logError("couldn't obtain global-level Vulkan function '{}'", #name); \
        return false; \
    }

#include "vulkan_function_list.inl"

    return true;
}

bool RenderingDriver::obtainExtensionProperties() {
    std::uint32_t extension_count = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    if (result != VK_SUCCESS || extension_count == 0) {
        logError("couldn't get the number of instance extensions");
        return false;
    }

    extensions_.resize(extension_count);
    result = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions_.data());
    if (result != VK_SUCCESS || extension_count == 0) {
        logError("couldn't enumerate Instance extensions");
        return false;
    }

    return true;
}

bool RenderingDriver::createInstance(const ApplicationInfo& app_info, const InstanceCreateInfo& create_info) {
    for (const char* extension : create_info.extensions) {
        if (!isExtensionSupported(extension)) {
            logError("instance extension '{}' is not supported", extension);
            return false;
        }
    }

    VkApplicationInfo vk_app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = app_info.name,
        .applicationVersion = VK_MAKE_VERSION(app_info.version.major, app_info.version.minor, app_info.version.patch),
        .pEngineName = "App3D Vulkan Rendering Driver",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_MAKE_VERSION(1, 0, 0),
    };

    VkInstanceCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &vk_app_info,
        .enabledExtensionCount = static_cast<uint32_t>(create_info.extensions.size()),
        .ppEnabledExtensionNames = !create_info.extensions.empty() ? create_info.extensions.data() : nullptr,
    };

    VkResult result = vkCreateInstance(&ci, nullptr, &instance_);
    if (result != VK_SUCCESS || instance_ == VK_NULL_HANDLE) {
        logError("couldn't create Vulkan Instance");
        return false;
    }

    const auto is_extension_enabled = [&create_info](const char* extension) {
        return std::ranges::any_of(create_info.extensions, [extension](const char* enabled_extension) {
            return std::string_view(extension) == std::string_view(enabled_extension);
        });
    };

#define INSTANCE_LEVEL_VK_FUNCTION(name) \
    name = (PFN_##name)vkGetInstanceProcAddr(instance_, #name); \
    if (!name) { \
        logError("couldn't obtain instance-level Vulkan function '{}'", #name); \
        return false; \
    }

#define INSTANCE_LEVEL_VK_FUNCTION_FROM_EXTENSION(name, extension) \
    if (is_extension_enabled(extension)) { \
        name = (PFN_##name)vkGetInstanceProcAddr(instance_, #name); \
        if (!name) { \
            logError("couldn't obtain instance-level Vulkan function '{}'", #name); \
            return false; \
        } \
    } else { \
        logError("instance extension '{}' is not enabled", extension); \
        return false; \
    }

#include "vulkan_function_list.inl"

    return true;
}

bool RenderingDriver::obtainPhysicalDeviceList() {
    std::uint32_t physical_device_count = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance_, &physical_device_count, nullptr);
    if (result != VK_SUCCESS || physical_device_count == 0) {
        logError("couldn't get the number of available physical devices");
        return false;
    }

    std::vector<VkPhysicalDevice> physical_device_list(physical_device_count);
    result = vkEnumeratePhysicalDevices(instance_, &physical_device_count, physical_device_list.data());
    if (result != VK_SUCCESS || physical_device_count == 0) {
        logError("couldn't enumerate physical devices");
        return false;
    }

    physical_devices_.reserve(physical_device_count);

    for (const auto& phys_dev_handle : physical_device_list) {
        auto physical_device = std::make_unique<PhysicalDevice>(phys_dev_handle);
        if (physical_device->obtainExtensionProperties() && physical_device->obtainFeaturesAndProperties()) {
            physical_devices_.emplace_back(std::move(physical_device));
        }
    }

    return true;
}

// --------------------------------------------------------
// PhysicalDevice class implementation

PhysicalDevice::PhysicalDevice(VkPhysicalDevice physical_device) : physical_device_(physical_device) {}

bool PhysicalDevice::isExtensionSupported(const char* extension) const {
    return std::ranges::any_of(extensions_, [extension](const auto& item) {
        return std::string_view(extension) == std::string_view(item.extensionName);
    });
}

std::uint32_t PhysicalDevice::findSuitableQueueFamily(VkQueueFlags flags, std::uint32_t n) const {
    auto it = std::ranges::find_if(queue_families_, [flags, &n](const auto& item) {
        if (item.queueCount > 0 && (item.queueFlags & flags) == flags) {
            if (n == 0) { return true; }
            --n;
        }
        return false;
    });
    return it != queue_families_.end() ? static_cast<std::uint32_t>(it - queue_families_.begin()) :
                                         INVALID_UINT32_VALUE;
}

bool PhysicalDevice::isSuitableDevice(const DesiredDeviceCaps& caps) const {
    if (!features_.geometryShader) { return false; }
    return true;
}

bool PhysicalDevice::obtainExtensionProperties() {
    std::uint32_t extension_count = 0;
    VkResult result = vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &extension_count, nullptr);
    if (result != VK_SUCCESS || extension_count == 0) {
        logError("couldn't get the number of device extensions");
        return false;
    }

    extensions_.resize(extension_count);
    result = vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &extension_count, extensions_.data());
    if (result != VK_SUCCESS || extension_count == 0) {
        logError("couldn't enumerate device extensions");
        return false;
    }

    return true;
}

bool PhysicalDevice::obtainFeaturesAndProperties() {
    vkGetPhysicalDeviceProperties(physical_device_, &properties_);
    vkGetPhysicalDeviceFeatures(physical_device_, &features_);

    std::uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, nullptr);
    if (queue_family_count == 0) {
        logError("couldn't get the number of queue families");
        return false;
    }

    queue_families_.resize(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, queue_families_.data());
    if (queue_family_count == 0) {
        logError("couldn't acquire properties of queue families");
        return false;
    }

    return true;
}

bool PhysicalDevice::createLogicalDevice(const DeviceCreateInfo& create_info, VkDevice& logical_device) {
    for (const char* extension : create_info.extensions) {
        if (!isExtensionSupported(extension)) {
            logError("device extension '{}' is not supported", extension);
            return false;
        }
    }

    std::vector<VkDeviceQueueCreateInfo> queue_cis;
    queue_cis.reserve(create_info.queue_infos.size());
    for (const auto& info : create_info.queue_infos) {
        queue_cis.push_back(VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = info.family_index,
            .queueCount = static_cast<uint32_t>(info.priorities.size()),
            .pQueuePriorities = !info.priorities.empty() ? info.priorities.data() : nullptr,
        });
    };

    VkDeviceCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = static_cast<uint32_t>(queue_cis.size()),
        .pQueueCreateInfos = !queue_cis.empty() ? queue_cis.data() : nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(create_info.extensions.size()),
        .ppEnabledExtensionNames = !create_info.extensions.empty() ? create_info.extensions.data() : nullptr,
        .pEnabledFeatures = &create_info.features,
    };

    VkResult result = vkCreateDevice(physical_device_, &ci, nullptr, &logical_device);
    if (result != VK_SUCCESS || logical_device == VK_NULL_HANDLE) {
        logError("couldn't create logical device");
        return false;
    }

    const auto is_extension_enabled = [&create_info](const char* extension) {
        return std::ranges::any_of(create_info.extensions, [extension](const char* enabled_extension) {
            return std::string_view(extension) == std::string_view(enabled_extension);
        });
    };

#define DEVICE_LEVEL_VK_FUNCTION(name) \
    name = (PFN_##name)vkGetDeviceProcAddr(logical_device, #name); \
    if (!name) { \
        logError("couldn't obtain device-level Vulkan function '{}'", #name); \
        return false; \
    }

#define DEVICE_LEVEL_VK_FUNCTION_FROM_EXTENSION(name, extension) \
    if (is_extension_enabled(extension)) { \
        name = (PFN_##name)vkGetDeviceProcAddr(logical_device, #name); \
        if (!name) { \
            logError("couldn't obtain instance-level Vulkan function '{}'", #name); \
            return false; \
        } \
    } else { \
        logError("device extension '{}' is not enabled", extension); \
        return false; \
    }

#include "vulkan_function_list.inl"

    return true;
}

APP3D_REGISTER_RENDERING_DRIVER("vulkan", app3d::rel::vulkan::RenderingDriver);
