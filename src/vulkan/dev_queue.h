#pragma once

#include "vulkan_api.h"

#include "common/core_defs.h"
#include "interfaces/i_rendering_driver.h"
#include "util/range_helpers.h"

#include <uxs/dynarray.h>

#include <vector>

namespace app3d::rel::vulkan {

class Device;
class CommandBuffer;

class DevQueue {
 public:
    DevQueue() = default;
    DevQueue(const DevQueue&) = delete;
    DevQueue& operator=(const DevQueue&) = delete;

    std::uint32_t getFamilyIndex() const { return family_index_; }
    void setFamilyIndex(std::uint32_t family_index) { family_index_ = family_index; }

    bool create(Device& device, std::uint32_t command_pool_count);
    bool growCommandPoolCount(std::uint32_t command_pool_count);
    bool resetCommandPool(std::uint32_t command_pool_index);
    void destroy();

    bool submitCommandBuffers(util::multispan<const VkSemaphore, const VkPipelineStageFlags> wait_semaphore_infos,
                              std::span<const VkCommandBuffer> command_buffers,
                              std::span<const VkSemaphore> signal_semaphores, VkFence fence);

    RenderTargetResult presentImages(std::span<const VkSemaphore> wait_semaphores,
                                     util::multispan<const VkSwapchainKHR, const std::uint32_t> images_to_present);

    bool obtainCommandBuffer(std::uint32_t command_pool_index, CommandBuffer& command_buffer);
    void releaseCommandBuffer(std::uint32_t command_pool_index, CommandBuffer& command_buffer);

 private:
    Device* device_ = nullptr;
    std::uint32_t family_index_ = INVALID_UINT32_VALUE;
    VkQueue queue_{VK_NULL_HANDLE};

    struct CommandPool {
        VkCommandPool command_pool_{VK_NULL_HANDLE};
        std::vector<VkCommandBuffer> allocated_command_buffers_;
        std::uint32_t used_command_buffer_count_ = 0;
    };

    uxs::inline_dynarray<CommandPool, 8> cmd_pools_;
};

}  // namespace app3d::rel::vulkan
