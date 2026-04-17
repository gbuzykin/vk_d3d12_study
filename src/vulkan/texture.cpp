#include "texture.h"

#include "device.h"
#include "object_destroyer.h"
#include "render_target.h"
#include "rendering_driver.h"
#include "vulkan_logger.h"

#include <array>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Texture class implementation

Texture::Texture(Device& device) : device_(util::not_null{&device}) {}

Texture::~Texture() {
    ObjectDestroyer<VkImageView>::destroy(~*device_, image_view_);
    vmaDestroyImage(device_->getAllocator(), image_, allocation_);
}

bool Texture::create(const TextureOpts& opts) {
    const std::uint32_t num_mipmaps = 1;
    const std::uint32_t num_layers = 1;
    const bool cubemap = false;

    image_usage_ = VK_IMAGE_USAGE_SAMPLED_BIT |
                   (opts.render_target_usage ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    image_format_ = VK_FORMAT_R8G8B8A8_UNORM;
    image_extent_ = {.width = opts.extent.width, .height = opts.extent.height, .depth = 1};

    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(~device_->getPhysicalDevice(), image_format_, &format_properties);

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
        .imageType = VK_IMAGE_TYPE_2D,
        .format = image_format_,
        .extent = image_extent_,
        .mipLevels = num_mipmaps,
        .arrayLayers = cubemap ? 6 * num_layers : num_layers,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = image_usage_,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    const VmaAllocationCreateInfo alloc_info{.usage = VMA_MEMORY_USAGE_AUTO};

    VkResult result = vmaCreateImage(device_->getAllocator(), &create_info, &alloc_info, &image_, &allocation_, nullptr);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create image: {}", result);
        return false;
    }

    const VkImageViewCreateInfo view_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image_,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = image_format_,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
    };

    result = vkCreateImageView(~*device_, &view_create_info, nullptr, &image_view_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create image view: {}", result);
        return false;
    }

    return true;
}

VkPipelineStageFlags Texture::getImageConsumingStages() const { return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; }

VkAccessFlags Texture::getImageAccess() const { return VK_ACCESS_SHADER_READ_BIT; }

VkImageLayout Texture::getImageLayout() const { return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; }

void Texture::imageBarrierBefore(CommandBuffer& command_buffer, std::uint32_t image_index) {}

void Texture::imageBarrierAfter(CommandBuffer& command_buffer, std::uint32_t image_index) {}

RenderTargetResult Texture::acquireFrameImage(std::uint32_t n_frame, std::uint64_t timeout, std::uint32_t& image_index) {
    image_index = 0;
    return RenderTargetResult::SUCCESS;
}

RenderTargetResult Texture::submitFrameImage(std::uint32_t n_frame, std::uint32_t image_index,
                                             CommandBuffer& command_buffer, VkFence fence) {
    if (!device_->getGraphicsQueue().submitCommandBuffers({}, std::array{~command_buffer}, {}, fence)) {
        return RenderTargetResult::FAILED;
    }
    return RenderTargetResult::SUCCESS;
}

//@{ ITexture

bool Texture::updateTexture(std::span<const std::uint8_t> data, Vec3i offset, Extent3u extent) {
    return device_->updateImage(data.data(), VkDeviceSize(data.size()), image_,
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
    auto render_target = util::make_new<RenderTarget>(*device_, *this);
    if (!render_target->create(opts)) { return nullptr; }
    return std::move(render_target);
}

//@}
