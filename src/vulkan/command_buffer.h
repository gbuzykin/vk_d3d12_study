#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"
#include "util/range_helpers.h"

namespace app3d::rel::vulkan {

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

    void copyBuffer(VkBuffer source_buffer, VkBuffer destination_buffer, std::span<const VkBufferCopy> regions) {
        vkCmdCopyBuffer(command_buffer_, source_buffer, destination_buffer, std::uint32_t(regions.size()),
                        regions.data());
    }

    void copyBufferToImage(VkBuffer source_buffer, VkImage destination_image, VkImageLayout image_layout,
                           std::span<const VkBufferImageCopy> regions) {
        vkCmdCopyBufferToImage(command_buffer_, source_buffer, destination_image, image_layout,
                               std::uint32_t(regions.size()), regions.data());
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

    void bindVertexBuffers(std::uint32_t first_binding, util::multispan<const VkBuffer, const VkDeviceSize> buffers) {
        vkCmdBindVertexBuffers(command_buffer_, first_binding, std::uint32_t(buffers.size()), buffers.data<0>(),
                               buffers.data<1>());
    }

    void bindDescriptorSets(VkPipelineBindPoint pipeline_type, VkPipelineLayout pipeline_layout,
                            std::uint32_t first_set_index, std::span<const VkDescriptorSet> descriptor_sets,
                            std::span<const std::uint32_t> dynamic_offsets) {
        vkCmdBindDescriptorSets(command_buffer_, pipeline_type, pipeline_layout, first_set_index,
                                std::uint32_t(descriptor_sets.size()), descriptor_sets.data(),
                                std::uint32_t(dynamic_offsets.size()), dynamic_offsets.data());
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

}  // namespace app3d::rel::vulkan
