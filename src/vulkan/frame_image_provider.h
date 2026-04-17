#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::vulkan {

class RenderTarget;
class CommandBuffer;

class FrameImageProvider : public util::ref_counter {
 public:
    virtual VkImageView getImageView(std::uint32_t image_index) = 0;
    virtual std::uint32_t getImageCount() const = 0;
    virtual std::uint32_t getFifCount() const = 0;
    virtual VkFormat getImageFormat() const = 0;
    virtual VkExtent2D getImageExtent() const = 0;
    virtual VkImageUsageFlags getImageUsage() const = 0;
    virtual VkPipelineStageFlags getImageConsumingStages() const = 0;
    virtual VkAccessFlags getImageAccess() const = 0;
    virtual VkImageLayout getImageLayout() const = 0;
    virtual void imageBarrierBefore(CommandBuffer& command_buffer, std::uint32_t image_index) = 0;
    virtual void imageBarrierAfter(CommandBuffer& command_buffer, std::uint32_t image_index) = 0;
    virtual RenderTargetResult acquireFrameImage(std::uint32_t n_frame, std::uint64_t timeout,
                                                 std::uint32_t& image_index) = 0;
    virtual RenderTargetResult submitFrameImage(std::uint32_t n_frame, std::uint32_t image_index,
                                                CommandBuffer& command_buffer, VkFence fence) = 0;
    virtual void removeRenderTarget(RenderTarget* render_target) = 0;
};

}  // namespace app3d::rel::vulkan
