#include "texture.h"

#include "device.h"
#include "object_destroyer.h"
#include "render_target.h"
#include "rendering_driver.h"
#include "vulkan_logger.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Texture class implementation

Texture::Texture(Device& device) : device_(device) {}

Texture::~Texture() {
    ObjectDestroyer<VkImageView>::destroy(~device_.get(), image_view_);
    vmaDestroyImage(device_.get().getAllocator(), image_, allocation_);
}

bool Texture::create(VkImageType type, VkFormat format, VkExtent3D extent, std::uint32_t num_mipmaps,
                     std::uint32_t num_layers, VkImageUsageFlags usage, bool cubemap, VkImageViewType view_type) {
    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(~device_.get().getPhysicalDevice(), format, &format_properties);

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
        .extent = extent,
        .mipLevels = num_mipmaps,
        .arrayLayers = cubemap ? 6 * num_layers : num_layers,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    const VmaAllocationCreateInfo alloc_info{.usage = VMA_MEMORY_USAGE_AUTO};

    VkResult result = vmaCreateImage(device_.get().getAllocator(), &create_info, &alloc_info, &image_, &allocation_,
                                     nullptr);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create image: {}", result);
        return false;
    }

    const VkImageViewCreateInfo view_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image_,
        .viewType = view_type,
        .format = format,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
    };

    result = vkCreateImageView(~device_.get(), &view_create_info, nullptr, &image_view_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create image view: {}", result);
        return false;
    }

    image_format_ = format;
    image_extent_ = extent;
    return true;
}

VkPipelineStageFlags Texture::getImageConsumingStages() const { return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; }

VkAccessFlags Texture::getImageAccess() const { return VK_ACCESS_SHADER_READ_BIT; }

VkImageLayout Texture::getImageLayout() const { return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; }

void Texture::imageBarrierBefore(CommandBuffer& command_buffer, std::uint32_t image_index) {}

bool Texture::imageBarrierAfter(CommandBuffer& command_buffer, std::uint32_t image_index) { return false; }

RenderTargetResult Texture::acquireImage(std::uint64_t timeout, VkSemaphore semaphore, std::uint32_t& image_index) {
    image_index = 0;
    return RenderTargetResult::SUCCESS;
}

RenderTargetResult Texture::presentImage(std::uint32_t n_frame, std::uint32_t image_index, VkSemaphore wait_semaphore,
                                         VkFence fence) {
    return RenderTargetResult::SUCCESS;
}

//@{ ITexture

bool Texture::updateTexture(std::span<const std::uint8_t> data, Vec3i offset, Extent3u extent) {
    return device_.get().updateImage(data.data(), VkDeviceSize(data.size()), image_,
                                     {
                                         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                         .mipLevel = 0,
                                         .baseArrayLayer = 0,
                                         .layerCount = 1,
                                     },
                                     {.x = offset.x, .y = offset.y, .z = offset.z},
                                     {.width = extent.width, .height = extent.height, .depth = extent.depth},
                                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                     VK_ACCESS_NONE, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, {});
}

util::ref_ptr<IRenderTarget> Texture::createRenderTarget(const uxs::db::value& opts) {
    util::ref_ptr render_target = ::new RenderTarget(device_, *this);
    if (!render_target->create(opts)) { return nullptr; }
    return std::move(render_target);
}

//@}
