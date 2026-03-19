#pragma once

#include "vulkan_api.h"

#include <cstdint>

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

}  // namespace app3d::rel::vulkan
