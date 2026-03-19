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
    VkDeviceMemory getMemoryObject() { return memory_object_; }
    VkImageView getImageView() { return image_view_; }

    //@{ ITexture
    bool updateTexture(std::span<const std::uint8_t> data, Vec3i offset, Extent3u extent) override;
    //@}

 private:
    Device& device_;
    VkImage image_{VK_NULL_HANDLE};
    VkDeviceMemory memory_object_{VK_NULL_HANDLE};
    VkImageView image_view_{VK_NULL_HANDLE};

    bool createImageView(VkImageViewType view_type, VkFormat format, VkImageAspectFlags aspect);
    bool allocateAndBindMemoryObjectToImage(VkMemoryPropertyFlagBits desired_properties);
};

}  // namespace app3d::rel::vulkan
