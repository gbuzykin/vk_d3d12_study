#pragma once

#include "command_buffer.h"
#include "dev_queue.h"

#include <vector>

namespace app3d::rel::vulkan {

class RenderingDriver;
class PhysicalDevice;
class ShaderModule;
class Pipeline;
class Buffer;

class MappedMemory {
 public:
    MappedMemory(VkDevice device, VkDeviceMemory memory_object) : device_(device), memory_object_(memory_object) {}
    ~MappedMemory() { release(); }
    MappedMemory(const MappedMemory&) = delete;
    MappedMemory& operator=(const MappedMemory&) = delete;

    void* ptr() const { return ptr_; }
    bool map(VkDeviceSize offset, VkDeviceSize data_size);
    void release() {
        if (!ptr_) { return; }
        vkUnmapMemory(device_, memory_object_);
        ptr_ = nullptr;
    }

    VkDeviceMemory operator~() { return memory_object_; }

 private:
    VkDevice device_;
    VkDeviceMemory memory_object_;
    void* ptr_ = nullptr;
};

class Device final : public IDevice {
 public:
    Device(RenderingDriver& instance, PhysicalDevice& physical_device);
    ~Device() override;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    bool create(const uxs::db::value& caps);
    void finalize();
    bool waitDevice();
    bool createSemaphore(VkSemaphore& semaphore);
    bool createFence(bool signaled, VkFence& fence);
    bool waitForFences(std::span<const VkFence> fences, VkBool32 wait_for_all, std::uint64_t timeout);
    bool resetFences(std::span<const VkFence> fences);
    VkCommandBuffer obtainCommandBuffer();
    void releaseCommandBuffer(VkCommandBuffer command_buffer);
    bool writeToDeviceLocalMemory(VkDeviceSize data_size, const void* data, VkBuffer dst, VkDeviceSize dst_offset,
                                  VkAccessFlags dst_current_access, VkAccessFlags dst_new_access,
                                  VkPipelineStageFlags dst_generating_stages, VkPipelineStageFlags dst_consuming_stages,
                                  std::span<const VkSemaphore> signal_semaphores);

    VkDevice operator~() { return device_; }
    PhysicalDevice& getPhysicalDevice() { return physical_device_; }
    DevQueue& getGraphicsQueue() { return graphics_queue_; }
    DevQueue& getPresentQueue() { return present_queue_; }
    DevQueue& getComputeQueue() { return compute_queue_; }

    //@{ IDevice
    IShaderModule* createShaderModule(std::span<const std::uint8_t> source_spirv,
                                      const uxs::db::value& create_info) override;
    IPipeline* createPipeline(IRenderTarget& render_target, std::span<IShaderModule* const> shader_modules,
                              const uxs::db::value& create_info) override;
    IBuffer* createBuffer(std::size_t size) override;
    //@}

 private:
    RenderingDriver& instance_;
    PhysicalDevice& physical_device_;
    VkDevice device_{VK_NULL_HANDLE};
    DevQueue graphics_queue_;
    DevQueue compute_queue_;
    DevQueue present_queue_;

    VkCommandPool command_pool_{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> command_buffers_;
    std::size_t used_command_buffers_count_ = 0;
    std::vector<std::unique_ptr<ShaderModule>> shader_modules_;
    std::vector<std::unique_ptr<Pipeline>> pipelines_;
    std::vector<std::unique_ptr<Buffer>> buffers_;
    CommandBuffer transfer_command_buffer_;

    bool createCommandPool(VkCommandPoolCreateFlags flags, std::uint32_t queue_family, VkCommandPool& command_pool);
    bool allocateCommandBuffers(VkCommandPool command_pool, VkCommandBufferLevel level,
                                std::span<VkCommandBuffer> command_buffers);

    bool writeToHostVisibleMemory(VkDeviceMemory memory_object, VkDeviceSize offset, VkDeviceSize data_size,
                                  const void* data);
};

}  // namespace app3d::rel::vulkan
