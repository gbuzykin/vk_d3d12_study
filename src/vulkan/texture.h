#pragma once

#include "frame_image_provider.h"

namespace app3d::rel::vulkan {

class Device;

class Texture final : public FrameImageProvider, public ITexture {
 public:
    explicit Texture(Device& device);
    ~Texture() override;

    bool create(const TextureDesc& desc);

    //@{ FrameImageProvider
    VkImageView getImageView(std::uint32_t image_index) override { return image_view_; }
    std::uint32_t getImageCount() const override { return 1; }
    std::uint32_t getFifCount() const override { return 1; }
    VkFormat getImageFormat() const override;
    VkExtent2D getImageExtent() const override {
        return {.width = image_extent_.width, .height = image_extent_.height};
    }
    VkImageUsageFlags getImageUsage() const override { return image_usage_; }
    VkPipelineStageFlags getImageConsumingStages() const override;
    VkAccessFlags getImageAccess() const override;
    VkImageLayout getImageLayout() const override;
    void imageBarrierBefore(CommandBuffer& command_buffer, std::uint32_t image_index) override;
    void imageBarrierAfter(CommandBuffer& command_buffer, std::uint32_t image_index) override;
    RenderTargetResult acquireFrameImage(std::uint64_t timeout, std::uint32_t& image_index) override;
    RenderTargetResult submitFrameImage(std::uint32_t image_index, CommandBuffer& command_buffer,
                                        VkFence fence) override;
    void removeRenderTarget(RenderTarget* render_target) override {}
    //@}

    //@{ ITexture
    util::ref_counter& getRefCounter() override { return *this; }
    bool updateTexture(const std::uint8_t* data, std::uint32_t first_subresource,
                       std::span<const UpdateTextureDesc> update_subresource_descs) override;
    util::ref_ptr<IRenderTarget> createRenderTarget(const uxs::db::value& opts) override;
    //@}

 private:
    util::ref_ptr<Device> device_;
    Format image_format_{};
    Extent3u image_extent_{};
    VkPipelineStageFlags image_usage_{};
    VkImage image_{VK_NULL_HANDLE};
    VmaAllocation allocation_{VK_NULL_HANDLE};
    VkImageView image_view_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
