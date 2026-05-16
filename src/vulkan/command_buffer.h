#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"
#include "util/range_helpers.h"

namespace app3d::rel::vulkan {

class CommandBuffer {
 public:
    CommandBuffer() = default;
    CommandBuffer(const DeviceVkFuncTable& vk_funcs, VkCommandBuffer command_buffer)
        : vk_funcs_(&vk_funcs), command_buffer_(command_buffer) {}

#define DEVICE_LEVEL_VK_FUNCTION_CMD(name) \
    template<typename... Args> \
    auto name(Args&&... args) { \
        return vk_funcs_->name(command_buffer_, std::forward<Args>(args)...); \
    }

#define DEVICE_LEVEL_VK_FUNCTION_FROM_EXTENSION_CMD(name, extension) \
    template<typename... Args> \
    auto name(Args&&... args) { \
        return vk_funcs_->name(command_buffer_, std::forward<Args>(args)...); \
    }

#include "vulkan_function_list.inl"

    bool beginCommandBuffer(VkCommandBufferUsageFlags usage,
                            VkCommandBufferInheritanceInfo* secondary_command_buffer_info);
    bool endCommandBuffer();

    void setImageMemoryBarrier(VkPipelineStageFlags generating_stages, VkPipelineStageFlags consuming_stages,
                               std::span<const VkImageMemoryBarrier> image_memory_barriers) {
        vkCmdPipelineBarrier(generating_stages, consuming_stages, 0, 0, nullptr, 0, nullptr,
                             std::uint32_t(image_memory_barriers.size()), image_memory_barriers.data());
    }

    void setBufferMemoryBarrier(VkPipelineStageFlags generating_stages, VkPipelineStageFlags consuming_stages,
                                std::span<const VkBufferMemoryBarrier> buffer_memory_barriers) {
        vkCmdPipelineBarrier(generating_stages, consuming_stages, 0, 0, nullptr,
                             std::uint32_t(buffer_memory_barriers.size()), buffer_memory_barriers.data(), 0, nullptr);
    }

    void copyBuffer(VkBuffer source_buffer, VkBuffer destination_buffer, std::span<const VkBufferCopy> regions) {
        vkCmdCopyBuffer(source_buffer, destination_buffer, std::uint32_t(regions.size()), regions.data());
    }

    void copyBufferToImage(VkBuffer source_buffer, VkImage destination_image, VkImageLayout image_layout,
                           std::span<const VkBufferImageCopy> regions) {
        vkCmdCopyBufferToImage(source_buffer, destination_image, image_layout, std::uint32_t(regions.size()),
                               regions.data());
    }

    void setViewports(std::uint32_t first_viewport, std::span<const VkViewport> viewports) {
        vkCmdSetViewport(first_viewport, std::uint32_t(viewports.size()), viewports.data());
    }

    void setScissors(std::uint32_t first_scissor, std::span<const VkRect2D> scissors) {
        vkCmdSetScissor(first_scissor, std::uint32_t(scissors.size()), scissors.data());
    }

    void bindVertexBuffers(std::uint32_t first_binding, util::multispan<const VkBuffer, const VkDeviceSize> buffers) {
        vkCmdBindVertexBuffers(first_binding, std::uint32_t(buffers.size()), buffers.data<0>(), buffers.data<1>());
    }

    void bindVertexBuffers2(
        std::uint32_t first_binding,
        util::multispan<const VkBuffer, const VkDeviceSize, const VkDeviceSize, const VkDeviceSize> buffers) {
        vkCmdBindVertexBuffers2EXT(first_binding, std::uint32_t(buffers.size()), buffers.data<0>(), buffers.data<1>(),
                                   buffers.data<2>(), buffers.data<3>());
    }

    void bindDescriptorSets(VkPipelineBindPoint pipeline_type, VkPipelineLayout pipeline_layout,
                            std::uint32_t first_set_index, std::span<const VkDescriptorSet> descriptor_sets,
                            std::span<const std::uint32_t> dynamic_offsets) {
        vkCmdBindDescriptorSets(pipeline_type, pipeline_layout, first_set_index, std::uint32_t(descriptor_sets.size()),
                                descriptor_sets.data(), std::uint32_t(dynamic_offsets.size()), dynamic_offsets.data());
    }

    void beginRenderPass(VkRenderPass render_pass, VkFramebuffer framebuffer, VkRect2D render_area,
                         VkSubpassContents subpass_contents, std::span<const VkClearValue> clear_values,
                         std::span<const VkImageView> attachments);

    VkCommandBuffer getHandle() { return command_buffer_; }

 private:
    const DeviceVkFuncTable* vk_funcs_;
    VkCommandBuffer command_buffer_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
