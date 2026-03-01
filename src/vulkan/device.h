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

    VkDevice getHandle() { return device_; }

    //@{ IDevice
    ISwapChain* createSwapChain(ISurface& surface, const SwapChainCreateInfo& create_info) override;
    bool createSemaphores(std::span<SemaphoreHandle> semaphores) override;
    void destroySemaphores(std::span<const SemaphoreHandle> semaphores) override;
    bool createFences(std::span<FenceHandle> fences) override;
    void destroyFences(std::span<const FenceHandle> fences) override;
    //@}

 private:
    RenderingDriver& instance_;
    PhysicalDevice& physical_device_;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;
    VkQueue compute_queue_ = VK_NULL_HANDLE;
};

}  // namespace app3d::rel::vulkan
