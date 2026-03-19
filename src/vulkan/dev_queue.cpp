#include "dev_queue.h"

#include "device.h"
#include "object_destroyer.h"

#include "common/logger.h"

#include <algorithm>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// DevQueue class implementation

DevQueue::DevQueue(Device& device) : device_(device) {}

DevQueue::~DevQueue() { destroy(); }

bool DevQueue::create() {
    if (family_index_ == INVALID_UINT32_VALUE) {
        logError(LOG_VK "not selected queue family");
        return false;
    }

    vkGetDeviceQueue(~device_, family_index_, 0, &queue_);

    const VkCommandPoolCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = family_index_,
    };

    VkResult result = vkCreateCommandPool(~device_, &create_info, nullptr, &command_pool_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create command pool");
        return false;
    }

    return true;
}

void DevQueue::destroy() {
    ObjectDestroyer<VkCommandPool>::destroy(~device_, command_pool_);
    command_pool_ = VK_NULL_HANDLE;
    allocated_command_buffers_.clear();
    used_command_buffer_count_ = 0;
}

bool DevQueue::submitCommandBuffers(util::multispan<const VkSemaphore, const VkPipelineStageFlags> wait_semaphore_infos,
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

RenderTargetResult DevQueue::presentImages(std::span<const VkSemaphore> wait_semaphores,
                                           util::multispan<const VkSwapchainKHR, const std::uint32_t> images_to_present) {
    const VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = std::uint32_t(wait_semaphores.size()),
        .pWaitSemaphores = wait_semaphores.data(),
        .swapchainCount = std::uint32_t(images_to_present.size()),
        .pSwapchains = images_to_present.data<0>(),
        .pImageIndices = images_to_present.data<1>(),
    };

    VkResult result = vkQueuePresentKHR(queue_, &present_info);
    if (result == VK_SUCCESS) { return RenderTargetResult::SUCCESS; }
    if (result == VK_SUBOPTIMAL_KHR) { return RenderTargetResult::SUBOPTIMAL; }
    if (result == VK_ERROR_OUT_OF_DATE_KHR) { return RenderTargetResult::OUT_OF_DATE; }
    return RenderTargetResult::FAILED;
}

bool DevQueue::obtainCommandBuffer(VkCommandBuffer& command_buffer) {
    if (used_command_buffer_count_ == allocated_command_buffers_.size()) {
        allocated_command_buffers_.resize(allocated_command_buffers_.size() + 5, VK_NULL_HANDLE);

        const VkCommandBufferAllocateInfo allocate_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = command_pool_,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = std::uint32_t(allocated_command_buffers_.size()) - used_command_buffer_count_,
        };

        VkResult result = vkAllocateCommandBuffers(~device_, &allocate_info,
                                                   allocated_command_buffers_.data() + used_command_buffer_count_);
        if (result != VK_SUCCESS) {
            logError(LOG_VK "couldn't allocate command buffers");
            return false;
        }
    }

    command_buffer = allocated_command_buffers_[used_command_buffer_count_++];

    VkResult result = vkResetCommandBuffer(command_buffer, 0);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't free command buffers");
        return false;
    }

    return true;
}

void DevQueue::releaseCommandBuffer(VkCommandBuffer command_buffer) {
    auto found_it = std::ranges::find(allocated_command_buffers_, command_buffer);
    if (found_it == allocated_command_buffers_.end()) { return; }
    std::copy(found_it + 1, allocated_command_buffers_.end(), found_it);
    allocated_command_buffers_.back() = command_buffer;
    --used_command_buffer_count_;
}
