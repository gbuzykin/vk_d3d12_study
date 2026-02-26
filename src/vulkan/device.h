#pragma once

#include "rendering_iface.h"
#include "vulkan_api.h"

namespace app3d::rel::vulkan {

class PhysicalDevice;

class Device : public IDevice {
 public:
    explicit Device(PhysicalDevice& physical_device);
    ~Device() override;

    bool create(const DesiredDeviceCaps& caps);

    //@{ IDevice
    //@}

 private:
    PhysicalDevice& physical_device_;
    VkDevice device_{};
    VkQueue graphics_queue_{};
    VkQueue compute_queue_{};
};

}  // namespace app3d::rel::vulkan
