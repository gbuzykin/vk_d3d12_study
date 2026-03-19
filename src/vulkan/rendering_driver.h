#pragma once

#include "vulkan_api.h"  // NOLINT

#include "interfaces/i_rendering_driver.h"

#include <vector>

namespace app3d::rel::vulkan {

class Surface;
class Device;

class PhysicalDevice {
 public:
    explicit PhysicalDevice(VkPhysicalDevice physical_device);
    PhysicalDevice(const PhysicalDevice&) = delete;
    PhysicalDevice& operator=(const PhysicalDevice&) = delete;

    bool isExtensionSupported(const char* extension) const;
    const VkPhysicalDeviceProperties& getProperties() const { return properties_; }
    const VkPhysicalDeviceFeatures& getFeatures() const { return features_; }
    const VkPhysicalDeviceMemoryProperties& getMemoryProperties() const { return memory_properties_; }
    const std::vector<VkQueueFamilyProperties>& getQueueFamilies() const { return queue_families_; }
    std::uint32_t findSuitableQueueFamily(VkQueueFlags flags, std::uint32_t n = 0) const;
    bool isSuitableDevice(const uxs::db::value& caps) const;

    bool loadExtensionProperties();
    bool loadFeaturesAndProperties();

    VkPhysicalDevice operator~() { return physical_device_; }

 private:
    VkPhysicalDevice physical_device_;
    std::vector<VkExtensionProperties> extensions_;
    VkPhysicalDeviceProperties properties_{};
    VkPhysicalDeviceFeatures features_{};
    VkPhysicalDeviceMemoryProperties memory_properties_{};
    std::vector<VkQueueFamilyProperties> queue_families_;
};

class RenderingDriver final : public IRenderingDriver {
 public:
    RenderingDriver();
    ~RenderingDriver() override;
    RenderingDriver(const RenderingDriver&) = delete;
    RenderingDriver& operator=(const RenderingDriver&) = delete;

    bool isExtensionSupported(const char* extension) const;
    const std::vector<std::unique_ptr<Surface>>& getSurfaces() { return surfaces_; }

    VkInstance operator~() { return instance_; }

    //@{ IRenderingDriver
    bool init(const uxs::db::value& app_info) override;
    std::uint32_t getPhysicalDeviceCount() const override;
    const char* getPhysicalDeviceName(std::uint32_t device_index) const override;
    bool isSuitablePhysicalDevice(std::uint32_t device_index, const uxs::db::value& caps) const override;
    ISurface* createSurface(const WindowDescriptor& win_desc) override;
    IDevice* createDevice(std::uint32_t device_index, const uxs::db::value& caps) override;
    //@}

 private:
    void* vulkan_library_ = nullptr;
    VkInstance instance_{VK_NULL_HANDLE};
    std::vector<VkExtensionProperties> extensions_;
    std::vector<std::unique_ptr<PhysicalDevice>> physical_devices_;
    std::vector<std::unique_ptr<Surface>> surfaces_;
    std::unique_ptr<Device> device_;

    bool loadVulkanLoaderLibrary();
    bool loadExtensionProperties();
    bool createInstance(const uxs::db::value& app_info);
    bool loadPhysicalDeviceList();
};

}  // namespace app3d::rel::vulkan
