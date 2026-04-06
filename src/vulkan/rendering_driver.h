#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

#include <vector>

namespace app3d::rel::vulkan {

class Surface;

class PhysicalDevice {
 public:
    explicit PhysicalDevice(VkPhysicalDevice physical_device);
    PhysicalDevice(const PhysicalDevice&) = delete;
    PhysicalDevice& operator=(const PhysicalDevice&) = delete;

    bool isExtensionSupported(const char* extension) const;
    const VkPhysicalDeviceProperties& getProperties() const { return properties_; }
    const VkPhysicalDeviceFeatures& getFeatures() const { return features_; }
    const VkPhysicalDeviceMemoryProperties& getMemoryProperties() const { return memory_properties_; }
    std::span<const VkQueueFamilyProperties> getQueueFamilies() const { return queue_families_; }
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

class RenderingDriver final : public util::ref_counter, public IRenderingDriver {
 public:
    RenderingDriver();
    ~RenderingDriver() override;

    bool isExtensionSupported(const char* extension) const;
    std::span<Surface*> getSurfaces() { return surfaces_; }
    void removeSurface(Surface* surface) { std::erase(surfaces_, surface); }

    VkInstance operator~() { return instance_; }

    //@{ IUnknown
    util::ref_counter& getRefCounter() override { return *this; }
    //@}

    //@{ IRenderingDriver
    bool init(const uxs::db::value& app_info) override;
    std::uint32_t getPhysicalDeviceCount() const override;
    const char* getPhysicalDeviceName(std::uint32_t device_index) const override;
    bool isSuitablePhysicalDevice(std::uint32_t device_index, const uxs::db::value& caps) const override;
    util::ref_ptr<ISurface> createSurface(const WindowDescriptor& win_desc) override;
    util::ref_ptr<IDevice> createDevice(std::uint32_t device_index, const uxs::db::value& caps) override;
    //@}

 private:
    void* vulkan_library_ = nullptr;
    VkInstance instance_{VK_NULL_HANDLE};
    std::vector<VkExtensionProperties> extensions_;
    std::vector<std::unique_ptr<PhysicalDevice>> physical_devices_;
    std::vector<Surface*> surfaces_;

    bool loadVulkanLoaderLibrary();
    bool loadExtensionProperties();
    bool createInstance(const uxs::db::value& app_info);
    bool loadPhysicalDeviceList();
};

}  // namespace app3d::rel::vulkan
