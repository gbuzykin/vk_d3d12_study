#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::vulkan {

class CommandBuffer;

class ImageProvider {
 public:
    virtual VkImage getImage(std::uint32_t image_index) = 0;
    virtual VkImageView getImageView(std::uint32_t image_index) = 0;
    virtual std::uint32_t getImageCount() const = 0;
    virtual std::uint32_t getFifCount() const = 0;
    virtual VkExtent2D getImageExtent() const = 0;
    virtual VkFormat getImageFormat() const = 0;
    virtual VkPipelineStageFlags getImageConsumingStages() const = 0;
    virtual VkAccessFlags getImageAccess() const = 0;
    virtual VkImageLayout getImageLayout() const = 0;
    virtual void imageBarrierBefore(CommandBuffer& command_buffer, std::uint32_t image_index) = 0;
    virtual bool imageBarrierAfter(CommandBuffer& command_buffer, std::uint32_t image_index) = 0;
    virtual RenderTargetResult acquireImage(std::uint64_t timeout, VkSemaphore semaphore,
                                            std::uint32_t& image_index) = 0;
    virtual RenderTargetResult presentImage(std::uint32_t n_frame, std::uint32_t image_index,
                                            VkSemaphore wait_semaphore, VkFence fence) = 0;
};

}  // namespace app3d::rel::vulkan
