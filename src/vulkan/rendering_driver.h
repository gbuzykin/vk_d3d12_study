#pragma once

#include "rendering_iface.h"
#include "vulkan_api.h"  // NOLINT

#include <vector>

namespace app3d::rel::vulkan {

constexpr std::uint32_t INVALID_UINT32_VALUE = static_cast<std::uint32_t>(-1);

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
    std::uint32_t findSuitableQueueFamily(VkQueueFlags flags) const;
    bool isSuitableDevice(const DesiredDeviceCaps& caps) const;

    bool obtainExtensionProperties();
    bool obtainFeaturesAndProperties();
    bool createLogicalDevice(const DeviceCreateInfo& create_info, VkDevice& logical_device);

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

    //@{ IRenderingDriver
    bool init(const ApplicationInfo& app_info) override;
    std::uint32_t getPhysicalDeviceCount() const override;
    const char* getPhysicalDeviceName(std::uint32_t device_index) const override;
    bool isSuitablePhysicalDevice(std::uint32_t device_index, const DesiredDeviceCaps& caps) const override;
    IDevice* createDevice(std::uint32_t device_index, const DesiredDeviceCaps& caps) override;
    //@}

 private:
    void* vulkan_library_{};
    VkInstance instance_{};
    std::vector<VkExtensionProperties> extensions_;
    std::vector<std::unique_ptr<PhysicalDevice>> physical_devices_;
    std::unique_ptr<IDevice> device_;

    bool loadVulkanLoaderLibrary();
    bool obtainExtensionProperties();
    bool createInstance(const ApplicationInfo& app_info, const InstanceCreateInfo& create_info);
    bool obtainPhysicalDeviceList();
};

}  // namespace app3d::rel::vulkan
