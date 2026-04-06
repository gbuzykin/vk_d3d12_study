#pragma once

#include "image_provider.h"

namespace app3d::rel::vulkan {

class Device;

class Texture final : public ImageProvider, public ITexture {
 public:
    explicit Texture(Device& device);
    ~Texture() override;

    bool create(VkImageType type, VkFormat format, VkExtent3D extent, std::uint32_t num_mipmaps,
                std::uint32_t num_layers, VkImageUsageFlags usage, bool cubemap, VkImageViewType view_type);

    VkImage operator~() { return image_; }
    VkImageView getImageView() { return image_view_; }

    //@{ ImageProvider
    VkImage getImage(std::uint32_t image_index) override { return image_; }
    VkImageView getImageView(std::uint32_t image_index) override { return image_view_; }
    std::uint32_t getImageCount() const override { return 1; }
    std::uint32_t getFifCount() const override { return 1; }
    VkExtent2D getImageExtent() const override {
        return {.width = image_extent_.width, .height = image_extent_.height};
    }
    VkFormat getImageFormat() const override { return image_format_; }
    VkPipelineStageFlags getImageConsumingStages() const override;
    VkAccessFlags getImageAccess() const override;
    VkImageLayout getImageLayout() const override;
    void imageBarrierBefore(CommandBuffer& command_buffer, std::uint32_t image_index) override;
    bool imageBarrierAfter(CommandBuffer& command_buffer, std::uint32_t image_index) override;
    RenderTargetResult acquireImage(std::uint64_t timeout, VkSemaphore semaphore, std::uint32_t& image_index) override;
    RenderTargetResult presentImage(std::uint32_t n_frame, std::uint32_t image_index, VkSemaphore wait_semaphore,
                                    VkFence fence) override;
    void removeRenderTarget(RenderTarget* render_target) override {}
    //@}

    //@{ IUnknown
    util::ref_counter& getRefCounter() override { return *this; }
    //@}

    //@{ ITexture
    bool updateTexture(std::span<const std::uint8_t> data, Vec3i offset, Extent3u extent) override;
    util::ref_ptr<IRenderTarget> createRenderTarget(const uxs::db::value& opts) override;
    //@}

 private:
    util::reference<Device> device_;
    VkExtent3D image_extent_{};
    VkFormat image_format_{};
    VkImage image_{VK_NULL_HANDLE};
    VmaAllocation allocation_{VK_NULL_HANDLE};
    VkImageView image_view_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
