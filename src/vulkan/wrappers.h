#pragma once

#include "vulkan_api.h"

#include <cassert>
#include <cstdint>
#include <span>

namespace app3d::rel::vulkan {

template<typename Ty>
struct Wrapper;

template<>
struct Wrapper<VkImageMemoryBarrier> {
    VkImage image;
    VkAccessFlags current_access;
    VkAccessFlags new_access;
    VkImageLayout current_layout;
    VkImageLayout new_layout;
    std::uint32_t current_queue_family;
    std::uint32_t new_queue_family;
    VkImageAspectFlags aspect;
    static VkImageMemoryBarrier unwrap(Wrapper wrapper) {
        return {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = wrapper.current_access,
            .dstAccessMask = wrapper.new_access,
            .oldLayout = wrapper.current_layout,
            .newLayout = wrapper.new_layout,
            .srcQueueFamilyIndex = wrapper.current_queue_family,
            .dstQueueFamilyIndex = wrapper.new_queue_family,
            .image = wrapper.image,
            .subresourceRange =
                {
                    .aspectMask = wrapper.aspect,
                    .baseMipLevel = 0,
                    .levelCount = VK_REMAINING_MIP_LEVELS,
                    .baseArrayLayer = 0,
                    .layerCount = VK_REMAINING_ARRAY_LAYERS,
                },
        };
    }
};

template<>
struct Wrapper<VkSubpassDescription> {
    VkPipelineBindPoint pipeline_type;
    std::span<const VkAttachmentReference> input_attachments;
    std::span<const VkAttachmentReference> color_attachments;
    std::span<const VkAttachmentReference> resolve_attachments;
    const VkAttachmentReference* depth_stencil_attachment;
    std::span<const std::uint32_t> preserve_attachments;
    static VkSubpassDescription unwrap(Wrapper wrapper) {
        assert(wrapper.resolve_attachments.empty() ||
               wrapper.resolve_attachments.size() == wrapper.color_attachments.size());
        return {
            .pipelineBindPoint = wrapper.pipeline_type,
            .inputAttachmentCount = std::uint32_t(wrapper.input_attachments.size()),
            .pInputAttachments = wrapper.input_attachments.data(),
            .colorAttachmentCount = std::uint32_t(wrapper.color_attachments.size()),
            .pColorAttachments = wrapper.color_attachments.data(),
            .pResolveAttachments = !wrapper.resolve_attachments.empty() ? wrapper.resolve_attachments.data() : nullptr,
            .pDepthStencilAttachment = wrapper.depth_stencil_attachment,
            .preserveAttachmentCount = std::uint32_t(wrapper.preserve_attachments.size()),
            .pPreserveAttachments = wrapper.preserve_attachments.data(),
        };
    }
};

}  // namespace app3d::rel::vulkan
