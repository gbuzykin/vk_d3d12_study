#pragma once

#include "vulkan_api.h"  // NOLINT

#include "interfaces/i_rendering_driver.h"

#include <vector>

#define LOG_VK "vulkan: "

namespace app3d::rel::vulkan {

class Device;

class PhysicalDevice {
 public:
    explicit PhysicalDevice(VkPhysicalDevice physical_device);

    const char* getName() const { return properties_.deviceName; }
    bool isExtensionSupported(const char* extension) const;
    std::uint32_t findSuitableQueueFamily(VkQueueFlags flags) const;
    bool isSuitableDevice(const DesiredDeviceCaps& caps) const;

    bool loadExtensionProperties();
    bool loadFeaturesAndProperties();

    VkPhysicalDevice operator~() { return physical_device_; }

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

    VkInstance operator~() { return instance_; }

    //@{ IRenderingDriver
    bool init(const ApplicationInfo& app_info) override;
    std::uint32_t getPhysicalDeviceCount() const override;
    const char* getPhysicalDeviceName(std::uint32_t device_index) const override;
    bool isSuitablePhysicalDevice(std::uint32_t device_index, const DesiredDeviceCaps& caps) const override;
    IDevice* createDevice(std::uint32_t device_index, const DesiredDeviceCaps& caps) override;
    //@}

 private:
    void* vulkan_library_ = nullptr;
    VkInstance instance_{VK_NULL_HANDLE};
    std::vector<VkExtensionProperties> extensions_;
    std::vector<std::unique_ptr<PhysicalDevice>> physical_devices_;
    std::unique_ptr<Device> device_;

    bool loadVulkanLoaderLibrary();
    bool loadExtensionProperties();
    bool createInstance(const ApplicationInfo& app_info);
    bool loadPhysicalDeviceList();
};

}  // namespace app3d::rel::vulkan
