#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::vulkan {

class RenderingDriver;
class PhysicalDevice;

class Device : public IDevice {
 public:
    Device(RenderingDriver& instance, PhysicalDevice& physical_device);
    ~Device() override;

    bool create(const DesiredDeviceCaps& caps);
    void finalize();
    bool waitDevice();

    VkDevice operator~() { return device_; }

    //@{ IDevice
    //@}

 private:
    struct DevQueue {
        std::uint32_t family_index;
        VkQueue handle;
    };

    RenderingDriver& instance_;
    PhysicalDevice& physical_device_;
    VkDevice device_{VK_NULL_HANDLE};
    DevQueue graphics_queue_{INVALID_UINT32_VALUE, VK_NULL_HANDLE};
    DevQueue compute_queue_{INVALID_UINT32_VALUE, VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
