#pragma once

#include "dev_queue.h"

#include <vector>

namespace app3d::rel::vulkan {

class RenderingDriver;
class PhysicalDevice;

class CommandBuffer {
 public:
    CommandBuffer() = default;
    explicit CommandBuffer(VkCommandBuffer command_buffer) : command_buffer_(command_buffer) {}

    bool beginCommandBuffer(VkCommandBufferUsageFlags usage,
                            VkCommandBufferInheritanceInfo* secondary_command_buffer_info);
    bool endCommandBuffer();

    void setImageMemoryBarrier(VkPipelineStageFlags generating_stages, VkPipelineStageFlags consuming_stages,
                               std::span<const VkImageMemoryBarrier> image_memory_barriers);

    VkCommandBuffer operator~() { return command_buffer_; }

 private:
    VkCommandBuffer command_buffer_{VK_NULL_HANDLE};
};

class Device : public IDevice {
 public:
    Device(RenderingDriver& instance, PhysicalDevice& physical_device);
    ~Device() override;

    bool create(const uxs::db::value& caps);
    void finalize();
    bool waitDevice();

    VkDevice operator~() { return device_; }
    PhysicalDevice& getPhysicalDevice() { return physical_device_; }

    //@{ IDevice
    bool prepareTestScene(ISurface& surface) override;
    bool renderTestScene(ISwapChain& swap_chain) override;
    //@}

 private:
    RenderingDriver& instance_;
    PhysicalDevice& physical_device_;
    VkDevice device_{VK_NULL_HANDLE};
    DevQueue graphics_queue_;
    DevQueue present_queue_;
    DevQueue compute_queue_;

    VkSemaphore sem_image_acquired_{VK_NULL_HANDLE};
    VkSemaphore sem_ready_to_present_{VK_NULL_HANDLE};
    VkCommandPool command_pool_{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> command_buffers_;
    CommandBuffer command_buffer_;

    bool createSemaphore(VkSemaphore& semaphore);
    bool createCommandPool(VkCommandPoolCreateFlags flags, std::uint32_t queue_family, VkCommandPool& command_pool);
    bool allocateCommandBuffers(VkCommandPool command_pool, VkCommandBufferLevel level,
                                std::span<VkCommandBuffer> command_buffers);
};

}  // namespace app3d::rel::vulkan
