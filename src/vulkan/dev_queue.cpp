#include "dev_queue.h"

#include "device.h"
#include "vulkan_logger.h"

#include <algorithm>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// DevQueue class implementation

bool DevQueue::create(Device& device, std::uint32_t command_pool_count) {
    if (family_index_ == INVALID_UINT32_VALUE) {
        logError(LOG_VK "no selected queue family");
        return false;
    }

    device_ = &device;
    device_->vkGetDeviceQueue(family_index_, 0, &queue_);

    return growCommandPoolCount(command_pool_count);
}

bool DevQueue::growCommandPoolCount(std::uint32_t command_pool_count) {
    const std::uint32_t old_command_pool_count = std::uint32_t(cmd_pools_.size());
    if (command_pool_count <= old_command_pool_count) { return true; }

    cmd_pools_.resize(command_pool_count);

    for (std::uint32_t n = old_command_pool_count; n < command_pool_count; ++n) {
        VkResult result = device_->vkCreateCommandPool(constAddressOf(VkCommandPoolCreateInfo{
                                                           .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                                           .queueFamilyIndex = family_index_,
                                                       }),
                                                       nullptr, &cmd_pools_[n].command_pool_);
        if (result != VK_SUCCESS) {
            logError(LOG_VK "couldn't create command pool: {}", result);
            return false;
        }

        cmd_pools_[n].allocated_command_buffers_.reserve(16);
    }

    return true;
}

bool DevQueue::resetCommandPool(std::uint32_t command_pool_index) {
    VkResult result = device_->vkResetCommandPool(cmd_pools_[command_pool_index].command_pool_, 0);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't reset command pool: {}", result);
        return false;
    }
    return true;
}

void DevQueue::destroy() {
    if (!device_) { return; }
    for (auto& cmd_pool : cmd_pools_) {
        device_->vkDestroyCommandPool(cmd_pool.command_pool_, nullptr);
        cmd_pool.command_pool_ = VK_NULL_HANDLE;
        cmd_pool.allocated_command_buffers_.clear();
        cmd_pool.used_command_buffer_count_ = 0;
    }
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

    VkResult result = device_->getVkFuncs().vkQueueSubmit(queue_, 1, &submit_info, fence);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "error occurred during command buffer submission: {}", result);
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

    VkResult result = device_->getVkFuncs().vkQueuePresentKHR(queue_, &present_info);
    if (result == VK_SUCCESS) { return RenderTargetResult::SUCCESS; }
    if (result == VK_SUBOPTIMAL_KHR) { return RenderTargetResult::SUBOPTIMAL; }
    if (result == VK_ERROR_OUT_OF_DATE_KHR) { return RenderTargetResult::OUT_OF_DATE; }
    return RenderTargetResult::FAILED;
}

bool DevQueue::obtainCommandBuffer(std::uint32_t command_pool_index, CommandBuffer& command_buffer) {
    auto& cmd_pool = cmd_pools_[command_pool_index];
    if (cmd_pool.used_command_buffer_count_ == cmd_pool.allocated_command_buffers_.size()) {
        cmd_pool.allocated_command_buffers_.resize(cmd_pool.allocated_command_buffers_.size() + 5, VK_NULL_HANDLE);

        const VkCommandBufferAllocateInfo allocate_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = cmd_pool.command_pool_,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = std::uint32_t(cmd_pool.allocated_command_buffers_.size()) -
                                  cmd_pool.used_command_buffer_count_,
        };

        VkResult result = device_->vkAllocateCommandBuffers(
            &allocate_info, cmd_pool.allocated_command_buffers_.data() + cmd_pool.used_command_buffer_count_);
        if (result != VK_SUCCESS) {
            logError(LOG_VK "couldn't allocate command buffers: {}", result);
            return false;
        }
    }

    command_buffer = CommandBuffer(device_->getVkFuncs(),
                                   cmd_pool.allocated_command_buffers_[cmd_pool.used_command_buffer_count_++]);

    return true;
}

void DevQueue::releaseCommandBuffer(std::uint32_t command_pool_index, CommandBuffer& command_buffer) {
    auto& cmd_pool = cmd_pools_[command_pool_index];
    if (command_buffer.getHandle() == VK_NULL_HANDLE) { return; }
    auto found_it = std::ranges::find(cmd_pool.allocated_command_buffers_, command_buffer.getHandle());
    if (found_it == cmd_pool.allocated_command_buffers_.end()) { return; }
    std::copy(found_it + 1, cmd_pool.allocated_command_buffers_.end(), found_it);
    cmd_pool.allocated_command_buffers_.back() = command_buffer.getHandle();
    --cmd_pool.used_command_buffer_count_;
}
