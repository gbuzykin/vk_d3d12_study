#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"
#include "utils/range_helpers.h"

namespace app3d::rel::vulkan {

class Device;

class DevQueue {
 public:
    DevQueue() = default;
    explicit DevQueue(std::uint32_t family_index) : family_index_(family_index) {}
    DevQueue(const DevQueue&) = delete;
    DevQueue& operator=(const DevQueue&) = delete;

    std::uint32_t getFamilyIndex() const { return family_index_; }
    void setFamilyIndex(std::uint32_t family_index) { family_index_ = family_index; }

    void loadQueueHandle(Device& device);

    bool submitCommandBuffers(MultiSpan<const VkSemaphore, const VkPipelineStageFlags> wait_semaphore_infos,
                              std::span<const VkCommandBuffer> command_buffers,
                              std::span<const VkSemaphore> signal_semaphores, VkFence fence);

    ResultCode presentImages(std::span<const VkSemaphore> rendering_semaphores,
                             MultiSpan<const VkSwapchainKHR, const std::uint32_t> images_to_present);

    VkQueue operator~() { return queue_; }

 private:
    std::uint32_t family_index_ = INVALID_UINT32_VALUE;
    VkQueue queue_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
