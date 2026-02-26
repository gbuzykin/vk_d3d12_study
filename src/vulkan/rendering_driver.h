#pragma once

#include "vulkan_api.h"  // NOLINT

#include "interfaces/i_rendering_driver.h"

#include <vector>

#define LOG_VK "vulkan: "
namespace app3d::rel::vulkan {

constexpr std::uint32_t INVALID_UINT32_VALUE = std::uint32_t(-1);

class Surface;
class Device;

struct QueueInfo {
    std::uint32_t family_index = 0;
    std::vector<float> priorities;
};

struct DeviceCreateInfo {
    std::vector<const char*> extensions;
    VkPhysicalDeviceFeatures features{};
    std::vector<QueueInfo> queue_infos;
};

struct InstanceCreateInfo {
    std::vector<const char*> extensions;
};

class PhysicalDevice {
 public:
    explicit PhysicalDevice(VkPhysicalDevice physical_device);

    const char* getName() const { return properties_.deviceName; }
    bool isExtensionSupported(const char* extension) const;
    const std::vector<VkQueueFamilyProperties>& getQueueFamilies() const { return queue_families_; }
    std::uint32_t findSuitableQueueFamily(VkQueueFlags flags, std::uint32_t n = 0) const;
    bool isSuitableDevice(const DesiredDeviceCaps& caps) const;

    bool obtainExtensionProperties();
    bool obtainFeaturesAndProperties();
    bool createLogicalDevice(const DeviceCreateInfo& create_info, VkDevice& logical_device);

    VkPhysicalDevice getHandle() { return physical_device_; }

 private:
    VkPhysicalDevice physical_device_;
    std::vector<VkExtensionProperties> extensions_;
    VkPhysicalDeviceProperties properties_{};
    VkPhysicalDeviceFeatures features_{};
    std::vector<VkQueueFamilyProperties> queue_families_;
};

class RenderingDriver : public IRenderingDriver {
 public:
    RenderingDriver();
    ~RenderingDriver() override;

    bool isExtensionSupported(const char* extension) const;
    std::vector<std::unique_ptr<Surface>>& getSurfaces() { return surfaces_; }
    void destroySwapChains();

    VkInstance getHandle() { return instance_; }

    //@{ IRenderingDriver
    bool init(const ApplicationInfo& app_info) override;
    std::uint32_t getPhysicalDeviceCount() const override;
    const char* getPhysicalDeviceName(std::uint32_t device_index) const override;
    bool isSuitablePhysicalDevice(std::uint32_t device_index, const DesiredDeviceCaps& caps) const override;
    ISurface* createSurface(const WindowHandle& window_handle) override;
    IDevice* createDevice(std::uint32_t device_index, const DesiredDeviceCaps& caps) override;
    //@}

 private:
    void* vulkan_library_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    std::vector<VkExtensionProperties> extensions_;
    std::vector<std::unique_ptr<PhysicalDevice>> physical_devices_;
    std::vector<std::unique_ptr<Surface>> surfaces_;
    std::unique_ptr<Device> device_;

    bool loadVulkanLoaderLibrary();
    bool obtainExtensionProperties();
    bool createInstance(const ApplicationInfo& app_info, const InstanceCreateInfo& create_info);
    bool obtainPhysicalDeviceList();
};

}  // namespace app3d::rel::vulkan
