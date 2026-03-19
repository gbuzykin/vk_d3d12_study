#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"
#include "util/range_helpers.h"

namespace app3d::rel::vulkan {

class Device;

class DevQueue {
 public:
    explicit DevQueue(Device& device);
    ~DevQueue();
    DevQueue(const DevQueue&) = delete;
    DevQueue& operator=(const DevQueue&) = delete;

    std::uint32_t getFamilyIndex() const { return family_index_; }
    void setFamilyIndex(std::uint32_t family_index) { family_index_ = family_index; }

    bool create();
    void destroy();
    VkCommandBuffer obtainCommandBuffer();
    void releaseCommandBuffer(VkCommandBuffer command_buffer);

    bool submitCommandBuffers(util::multispan<const VkSemaphore, const VkPipelineStageFlags> wait_semaphore_infos,
                              std::span<const VkCommandBuffer> command_buffers,
                              std::span<const VkSemaphore> signal_semaphores, VkFence fence);

    RenderTargetResult presentImages(std::span<const VkSemaphore> rendering_semaphores,
                                     util::multispan<const VkSwapchainKHR, const std::uint32_t> images_to_present);

    VkQueue operator~() { return queue_; }

 private:
    Device& device_;
    std::uint32_t family_index_ = INVALID_UINT32_VALUE;
    VkQueue queue_{VK_NULL_HANDLE};
    VkCommandPool command_pool_{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> command_buffers_;
    std::size_t used_command_buffers_count_ = 0;
};

}  // namespace app3d::rel::vulkan
