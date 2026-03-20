#include "texture.h"

#include "device.h"
#include "object_destroyer.h"
#include "rendering_driver.h"
#include "vulkan_logger.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Texture class implementation

Texture::Texture(Device& device) : device_(device) {}

Texture::~Texture() {
    ObjectDestroyer<VkImageView>::destroy(~device_, image_view_);
    ObjectDestroyer<VkDeviceMemory>::destroy(~device_, memory_object_);
    ObjectDestroyer<VkImage>::destroy(~device_, image_);
}

bool Texture::create(VkImageType type, VkFormat format, VkExtent3D size, std::uint32_t num_mipmaps,
                     std::uint32_t num_layers, VkImageUsageFlags usage, bool cubemap, VkImageViewType view_type) {
    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(~device_.getPhysicalDevice(), format, &format_properties);

    if (!(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
        logError(LOG_VK "provided format is not supported for a sampled image");
        return false;
    }

    if (!(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        logError(LOG_VK "provided format is not supported for a linear image filtering");
        return false;
    }

    const VkImageCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = cubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0U,
        .imageType = type,
        .format = format,
        .extent = size,
        .mipLevels = num_mipmaps,
        .arrayLayers = cubemap ? 6 * num_layers : num_layers,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkResult result = vkCreateImage(~device_, &create_info, nullptr, &image_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create image: {}", result);
        return false;
    }

    if (!allocateAndBindMemoryObjectToImage(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) { return false; }

    if (!createImageView(view_type, format, VK_IMAGE_ASPECT_COLOR_BIT)) { return false; }

    return true;
}

//@{ ITexture

bool Texture::updateTexture(std::span<const std::uint8_t> data, Vec3i offset, Extent3u extent) {
    return device_.writeImageInDeviceLocalMemory(
        VkDeviceSize(data.size()), data.data(), image_,
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        {.x = offset.x, .y = offset.y, .z = offset.z},
        {.width = extent.width, .height = extent.height, .depth = extent.depth}, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, {});
}

//@}

bool Texture::createImageView(VkImageViewType view_type, VkFormat format, VkImageAspectFlags aspect) {
    const VkImageViewCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image_,
        .viewType = view_type,
        .format = format,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = aspect,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
    };

    VkResult result = vkCreateImageView(~device_, &create_info, nullptr, &image_view_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create image view: {}", result);
        return false;
    }

    return true;
}

bool Texture::allocateAndBindMemoryObjectToImage(VkMemoryPropertyFlagBits desired_properties) {
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(~device_, image_, &memory_requirements);

    const auto& memory_properties = device_.getPhysicalDevice().getMemoryProperties();

    for (std::uint32_t type = 0; type < memory_properties.memoryTypeCount; ++type) {
        if ((memory_requirements.memoryTypeBits & (1U << type)) &&
            (memory_properties.memoryTypes[type].propertyFlags & desired_properties) == desired_properties) {
            const VkMemoryAllocateInfo allocate_info{
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = memory_requirements.size,
                .memoryTypeIndex = type,
            };

            VkResult result = vkAllocateMemory(~device_, &allocate_info, nullptr, &memory_object_);
            if (result == VK_SUCCESS) { break; }
        }
    }

    if (memory_object_ == VK_NULL_HANDLE) {
        logError(LOG_VK "couldn't allocate memory for image");
        return false;
    }

    VkResult result = vkBindImageMemory(~device_, image_, memory_object_, 0);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't bind memory object to image: {}", result);
        return false;
    }

    return true;
}
