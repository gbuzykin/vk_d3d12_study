#include "dev_queue.h"

#include "device.h"

#include "utils/logger.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// DevQueue class implementation

void DevQueue::loadQueueHandle(Device& device) {
    if (family_index_ != INVALID_UINT32_VALUE) { vkGetDeviceQueue(~device, family_index_, 0, &queue_); }
}

bool DevQueue::submitCommandBuffers(MultiSpan<const VkSemaphore, const VkPipelineStageFlags> wait_semaphore_infos,
                                    std::span<const VkCommandBuffer> command_buffers,
                                    std::span<const VkSemaphore> signal_semaphores, VkFence fence) {
    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = std::uint32_t(wait_semaphore_infos.size()),
        .pWaitSemaphores = wait_semaphore_infos.data<0>(),
        .pWaitDstStageMask = wait_semaphore_infos.data<1>(),
        .commandBufferCount = std::uint32_t(command_buffers.size()),
        .pCommandBuffers = command_buffers.data(),
        .signalSemaphoreCount = std::uint32_t(signal_semaphores.size()),
        .pSignalSemaphores = signal_semaphores.data(),
    };

    VkResult result = vkQueueSubmit(queue_, 1, &submit_info, fence);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "error occurred during command buffer submission");
        return false;
    }

    return true;
}

ResultCode DevQueue::presentImages(std::span<const VkSemaphore> rendering_semaphores,
                                   MultiSpan<const VkSwapchainKHR, const std::uint32_t> images_to_present) {
    const VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = std::uint32_t(rendering_semaphores.size()),
        .pWaitSemaphores = rendering_semaphores.data(),
        .swapchainCount = std::uint32_t(images_to_present.size()),
        .pSwapchains = images_to_present.data<0>(),
        .pImageIndices = images_to_present.data<1>(),
    };

    VkResult result = vkQueuePresentKHR(queue_, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) { return ResultCode::OUT_OF_DATE; }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) { return ResultCode::FAILED; }
    return ResultCode::SUCCESS;
}
