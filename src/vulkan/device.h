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

    void beginRenderPass(VkRenderPass render_pass, VkFramebuffer framebuffer, VkRect2D render_area,
                         std::span<const VkClearValue> clear_values, VkSubpassContents subpass_contents);
    void endRenderPass() { vkCmdEndRenderPass(command_buffer_); }

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
    VkFence fence_drawing_{VK_NULL_HANDLE};
    VkCommandPool command_pool_{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> command_buffers_;
    CommandBuffer command_buffer_;
    VkRenderPass render_pass_{VK_NULL_HANDLE};

    std::uint32_t n_frame_ = 0;
    std::vector<VkSemaphore> sem_ready_to_present_;

    bool createSemaphore(VkSemaphore& semaphore);

    bool createFence(bool signaled, VkFence& fence);
    bool waitForFences(std::span<const VkFence> fences, VkBool32 wait_for_all, std::uint64_t timeout);
    bool resetFences(std::span<const VkFence> fences);

    bool createRenderPass(std::span<const VkAttachmentDescription> attachments_descriptions,
                          std::span<const VkSubpassDescription> subpass_descriptions,
                          std::span<const VkSubpassDependency> subpass_dependencies, VkRenderPass& render_pass);

    bool createFramebuffer(VkRenderPass render_pass, std::span<const VkImageView> attachments, VkExtent2D size,
                           std::uint32_t layers, VkFramebuffer& framebuffer);

    bool createCommandPool(VkCommandPoolCreateFlags flags, std::uint32_t queue_family, VkCommandPool& command_pool);
    bool allocateCommandBuffers(VkCommandPool command_pool, VkCommandBufferLevel level,
                                std::span<VkCommandBuffer> command_buffers);
};

}  // namespace app3d::rel::vulkan
