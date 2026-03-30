#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::vulkan {

class Device;

class Texture final : public ITexture {
 public:
    explicit Texture(Device& device);
    ~Texture() override;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    bool create(VkImageType type, VkFormat format, VkExtent3D size, std::uint32_t num_mipmaps, std::uint32_t num_layers,
                VkImageUsageFlags usage, bool cubemap, VkImageViewType view_type);

    VkImage operator~() { return image_; }
    VkImageView getImageView() { return image_view_; }

    //@{ ITexture
    bool updateTexture(std::span<const std::uint8_t> data, Vec3i offset, Extent3u extent) override;
    //@}

 private:
    Device& device_;
    VkImage image_{VK_NULL_HANDLE};
    VmaAllocation allocation_{VK_NULL_HANDLE};
    VkImageView image_view_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
