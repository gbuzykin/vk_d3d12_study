#include "rendering_driver.h"

#include "device.h"
#include "object_destroyer.h"
#include "surface.h"
#include "vulkan_logger.h"

#include "common/dynamic_library.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// RenderingDriver class implementation

RenderingDriver::RenderingDriver() {}

RenderingDriver::~RenderingDriver() {
    logDebug("destroy RenderingDriver");
    ObjectDestroyer<VkInstance>::destroy(instance_);
    if (vulkan_library_) { freeDynamicLibrary(vulkan_library_); }
}

bool RenderingDriver::isExtensionSupported(const char* extension) const {
    return std::ranges::any_of(extensions_, [extension](const auto& item) {
        return std::string_view(extension) == std::string_view(item.extensionName);
    });
}

//@{ IRenderingDriver

bool RenderingDriver::init(const uxs::db::value& app_info) {
    if (!loadVulkanLoaderLibrary()) { return false; }
    if (!loadExtensionProperties()) { return false; }
    if (!createInstance(app_info)) { return false; }
    if (!loadPhysicalDeviceList()) { return false; }

    surfaces_.reserve(4);
    return true;
}

std::uint32_t RenderingDriver::getPhysicalDeviceCount() const { return std::uint32_t(physical_devices_.size()); }

const char* RenderingDriver::getPhysicalDeviceName(std::uint32_t device_index) const {
    return physical_devices_[device_index]->getProperties().deviceName;
}

bool RenderingDriver::isSuitablePhysicalDevice(std::uint32_t device_index, const uxs::db::value& caps) const {
    return physical_devices_[device_index]->isSuitableDevice(caps);
}

util::ref_ptr<ISurface> RenderingDriver::createSurface(const WindowDescriptor& win_desc) {
    util::ref_ptr surface = ::new Surface(*this);
    if (!surface->create(win_desc)) { return nullptr; }
    surfaces_.push_back(surface.get());
    return std::move(surface);
}

util::ref_ptr<IDevice> RenderingDriver::createDevice(std::uint32_t device_index, const uxs::db::value& caps) {
    assert(!surfaces_.empty());

    auto& physical_device = *physical_devices_[device_index];

    for (auto* surface : surfaces_) {
        if (!surface->loadCapabilities(physical_device)) { return nullptr; }
        if (!surface->loadFormats(physical_device)) { return nullptr; }
        if (!surface->loadPresentQueueFamilies(physical_device)) { return nullptr; }
        if (!surface->loadPresentModes(physical_device)) { return nullptr; }
        if (!surface->checkAndSelectSurfaceFeatures()) { return nullptr; }
    }

    util::ref_ptr device = ::new Device(*this, physical_device);
    if (!device->create(caps)) { return nullptr; }
    return std::move(device);
}

//@}

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
        logError(LOG_VK "couldn't obtain global-level Vulkan function '{}'", #name); \
        return false; \
    }

#include "vulkan_function_list.inl"

    return true;
}

bool RenderingDriver::loadExtensionProperties() {
    std::uint32_t extension_count = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    if (result != VK_SUCCESS || extension_count == 0) {
        logError(LOG_VK "couldn't get the number of instance extensions: {}", result);
        return false;
    }

    extensions_.resize(extension_count);
    result = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions_.data());
    if (result != VK_SUCCESS || extension_count == 0) {
        logError(LOG_VK "couldn't enumerate Instance extensions: {}", result);
        return false;
    }

    return true;
}

bool RenderingDriver::createInstance(const uxs::db::value& app_info) {
    std::vector<const char*> instance_extensions;
    instance_extensions.reserve(32);
    instance_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef VK_USE_PLATFORM_WIN32_KHR
    instance_extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
    instance_extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
#ifdef VK_USE_PLATFORM_XCB_KHR
    instance_extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    instance_extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif

    for (const char* extension : instance_extensions) {
        if (!isExtensionSupported(extension)) {
            logError(LOG_VK "instance extension '{}' is not supported", extension);
            return false;
        }
    }

    const auto app_name = app_info.value<std::string>("name");
    const auto version = app_info.value("version");

    const VkApplicationInfo vk_app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = app_name.c_str(),
        .applicationVersion = VK_MAKE_VERSION(version.value<std::uint32_t>("major"),
                                              version.value<std::uint32_t>("minor"),
                                              version.value<std::uint32_t>("patch")),
        .pEngineName = "App3D Vulkan Rendering Driver",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_2,
    };

    const VkInstanceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &vk_app_info,
        .enabledExtensionCount = std::uint32_t(instance_extensions.size()),
        .ppEnabledExtensionNames = instance_extensions.data(),
    };

    VkResult result = vkCreateInstance(&create_info, nullptr, &instance_);
    if (result != VK_SUCCESS || instance_ == VK_NULL_HANDLE) {
        logError(LOG_VK "couldn't create Vulkan Instance: {}", result);
        return false;
    }

    const auto is_extension_enabled = [&instance_extensions](const char* extension) {
        return std::ranges::any_of(instance_extensions, [extension](const char* enabled_extension) {
            return std::string_view(extension) == std::string_view(enabled_extension);
        });
    };

#define INSTANCE_LEVEL_VK_FUNCTION(name) \
    name = (PFN_##name)vkGetInstanceProcAddr(instance_, #name); \
    if (!name) { \
        logError(LOG_VK "couldn't obtain instance-level Vulkan function '{}'", #name); \
        return false; \
    }

#define INSTANCE_LEVEL_VK_FUNCTION_FROM_EXTENSION(name, extension) \
    if (is_extension_enabled(extension)) { \
        name = (PFN_##name)vkGetInstanceProcAddr(instance_, #name); \
        if (!name) { \
            logError(LOG_VK "couldn't obtain instance-level Vulkan function '{}'", #name); \
            return false; \
        } \
    } else { \
        logError(LOG_VK "instance extension '{}' is not enabled", extension); \
        return false; \
    }

#include "vulkan_function_list.inl"

    return true;
}

bool RenderingDriver::loadPhysicalDeviceList() {
    std::uint32_t physical_device_count = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance_, &physical_device_count, nullptr);
    if (result != VK_SUCCESS || physical_device_count == 0) {
        logError(LOG_VK "couldn't get the number of available physical devices: {}", result);
        return false;
    }

    uxs::inline_dynarray<VkPhysicalDevice> physical_device_list(physical_device_count);
    result = vkEnumeratePhysicalDevices(instance_, &physical_device_count, physical_device_list.data());
    if (result != VK_SUCCESS || physical_device_count == 0) {
        logError(LOG_VK "couldn't enumerate physical devices: {}", result);
        return false;
    }

    physical_devices_.reserve(physical_device_count);

    for (const auto& phys_dev_handle : physical_device_list) {
        auto physical_device = std::make_unique<PhysicalDevice>(phys_dev_handle);
        if (physical_device->loadExtensionProperties() && physical_device->loadFeaturesAndProperties()) {
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
    return it != queue_families_.end() ? std::uint32_t(it - queue_families_.begin()) : INVALID_UINT32_VALUE;
}

bool PhysicalDevice::isSuitableDevice(const uxs::db::value& caps) const {
    if (!features_.geometryShader) { return false; }
    return true;
}

bool PhysicalDevice::loadExtensionProperties() {
    std::uint32_t extension_count = 0;
    VkResult result = vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &extension_count, nullptr);
    if (result != VK_SUCCESS || extension_count == 0) {
        logError(LOG_VK "couldn't get the number of device extensions: {}", result);
        return false;
    }

    extensions_.resize(extension_count);
    result = vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &extension_count, extensions_.data());
    if (result != VK_SUCCESS || extension_count == 0) {
        logError(LOG_VK "couldn't enumerate device extensions: {}", result);
        return false;
    }

    return true;
}

bool PhysicalDevice::loadFeaturesAndProperties() {
    vkGetPhysicalDeviceProperties(physical_device_, &properties_);
    vkGetPhysicalDeviceFeatures(physical_device_, &features_);
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties_);

    std::uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, nullptr);
    if (queue_family_count == 0) {
        logError(LOG_VK "couldn't get the number of queue families");
        return false;
    }

    queue_families_.resize(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, queue_families_.data());
    if (queue_family_count == 0) {
        logError(LOG_VK "couldn't acquire properties of queue families");
        return false;
    }

    return true;
}

APP3D_REGISTER_RENDERING_DRIVER("Vulkan", app3d::rel::vulkan::RenderingDriver);
