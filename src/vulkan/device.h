#pragma once

#include "dev_queue.h"

#include <vector>

namespace app3d::rel::vulkan {

class RenderingDriver;
class PhysicalDevice;
class Device;

class CommandBuffer {
 public:
    CommandBuffer() = default;

    static CommandBuffer wrap(VkCommandBuffer command_buffer) { return CommandBuffer{command_buffer}; }

    bool beginCommandBuffer(VkCommandBufferUsageFlags usage,
                            VkCommandBufferInheritanceInfo* secondary_command_buffer_info);
    bool endCommandBuffer();

    void setImageMemoryBarrier(VkPipelineStageFlags generating_stages, VkPipelineStageFlags consuming_stages,
                               std::span<const VkImageMemoryBarrier> image_memory_barriers) {
        vkCmdPipelineBarrier(command_buffer_, generating_stages, consuming_stages, 0, 0, nullptr, 0, nullptr,
                             std::uint32_t(image_memory_barriers.size()), image_memory_barriers.data());
    }

    void setBufferMemoryBarrier(VkPipelineStageFlags generating_stages, VkPipelineStageFlags consuming_stages,
                                std::span<const VkBufferMemoryBarrier> buffer_memory_barriers) {
        vkCmdPipelineBarrier(command_buffer_, generating_stages, consuming_stages, 0, 0, nullptr,
                             std::uint32_t(buffer_memory_barriers.size()), buffer_memory_barriers.data(), 0, nullptr);
    }

    void copyDataBetweenBuffers(VkBuffer source_buffer, VkBuffer destination_buffer,
                                std::span<const VkBufferCopy> regions) {
        vkCmdCopyBuffer(command_buffer_, source_buffer, destination_buffer, std::uint32_t(regions.size()),
                        regions.data());
    }

    void bindPipelineObject(VkPipelineBindPoint pipeline_type, VkPipeline pipeline) {
        vkCmdBindPipeline(command_buffer_, pipeline_type, pipeline);
    }

    void setViewportState(std::uint32_t first_viewport, std::span<const VkViewport> viewports) {
        vkCmdSetViewport(command_buffer_, first_viewport, std::uint32_t(viewports.size()), viewports.data());
    }

    void setScissorState(std::uint32_t first_scissor, std::span<const VkRect2D> scissors) {
        vkCmdSetScissor(command_buffer_, first_scissor, std::uint32_t(scissors.size()), scissors.data());
    }

    void bindVertexBuffers(std::uint32_t first_binding, MultiSpan<const VkBuffer, const VkDeviceSize> buffers) {
        vkCmdBindVertexBuffers(command_buffer_, first_binding, std::uint32_t(buffers.size()), buffers.data<0>(),
                               buffers.data<1>());
    }

    void drawGeometry(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex,
                      std::uint32_t first_instance) {
        vkCmdDraw(command_buffer_, vertex_count, instance_count, first_vertex, first_instance);
    }

    void beginRenderPass(VkRenderPass render_pass, VkFramebuffer framebuffer, VkRect2D render_area,
                         std::span<const VkClearValue> clear_values, VkSubpassContents subpass_contents);
    void endRenderPass() { vkCmdEndRenderPass(command_buffer_); }

    VkCommandBuffer operator~() { return command_buffer_; }

 private:
    explicit CommandBuffer(VkCommandBuffer command_buffer) : command_buffer_(command_buffer) {}
    VkCommandBuffer command_buffer_{VK_NULL_HANDLE};
};

class MappedMemory {
 public:
    MappedMemory(VkDevice device, VkDeviceMemory memory_object) : device_(device), memory_object_(memory_object) {}
    ~MappedMemory() { release(); }

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
    RenderTargetResult renderTestScene(ISwapChain& swap_chain) override;
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
    VkFence fence_drawing_{VK_NULL_HANDLE};
    VkCommandPool command_pool_{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> command_buffers_;
    CommandBuffer command_buffer_;
    VkRenderPass render_pass_{VK_NULL_HANDLE};
    VkShaderModule vertex_shader_module_{VK_NULL_HANDLE};
    VkShaderModule fragment_shader_module_{VK_NULL_HANDLE};
    VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
    std::vector<VkPipeline> graphics_pipelines_;
    VkPipeline graphics_pipeline_{VK_NULL_HANDLE};
    VkBuffer vertex_buffer_{VK_NULL_HANDLE};
    VkDeviceMemory vertex_buffer_memory_{VK_NULL_HANDLE};

    bool createSemaphore(VkSemaphore& semaphore);

    bool createFence(bool signaled, VkFence& fence);
    bool waitForFences(std::span<const VkFence> fences, VkBool32 wait_for_all, std::uint64_t timeout);
    bool resetFences(std::span<const VkFence> fences);

    bool createRenderPass(std::span<const VkAttachmentDescription> attachments_descriptions,
                          std::span<const VkSubpassDescription> subpass_descriptions,
                          std::span<const VkSubpassDependency> subpass_dependencies, VkRenderPass& render_pass);

    bool createFramebuffer(VkRenderPass render_pass, std::span<const VkImageView> attachments, VkExtent2D extent,
                           std::uint32_t layers, VkFramebuffer& framebuffer);

    bool createCommandPool(VkCommandPoolCreateFlags flags, std::uint32_t queue_family, VkCommandPool& command_pool);
    bool allocateCommandBuffers(VkCommandPool command_pool, VkCommandBufferLevel level,
                                std::span<VkCommandBuffer> command_buffers);

    bool createShaderModule(std::span<std::uint8_t> source_code, VkShaderModule& shader_module);

    bool createPipelineLayout(std::span<const VkDescriptorSetLayout> descriptor_set_layouts,
                              std::span<const VkPushConstantRange> push_constant_ranges,
                              VkPipelineLayout& pipeline_layout);

    bool createGraphicsPipelines(MultiSpan<const VkGraphicsPipelineCreateInfo, VkPipeline> pipelines,
                                 VkPipelineCache pipeline_cache);

    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer);
    bool allocateAndBindMemoryObjectToBuffer(VkBuffer buffer, VkMemoryPropertyFlagBits desired_properties,
                                             VkDeviceMemory& memory_object);
    bool writeToDeviceLocalMemory(DevQueue& queue, CommandBuffer& command_buffer, VkDeviceSize data_size, void* data,
                                  VkBuffer dst, VkDeviceSize dst_offset, VkAccessFlags dst_current_access,
                                  VkAccessFlags dst_new_access, VkPipelineStageFlags dst_generating_stages,
                                  VkPipelineStageFlags dst_consuming_stages,
                                  std::span<const VkSemaphore> signal_semaphores);
    bool writeToHostVisibleMemory(VkDeviceMemory memory_object, VkDeviceSize offset, VkDeviceSize data_size, void* data);
};

}  // namespace app3d::rel::vulkan
