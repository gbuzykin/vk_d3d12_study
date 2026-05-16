#include "render_target.h"

#include "descriptor_set.h"
#include "device.h"
#include "pipeline.h"
#include "swap_chain.h"
#include "tables.h"
#include "vulkan_logger.h"
#include "wrappers.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// RenderTarget class implementation

RenderTarget::RenderTarget(Device& device, FrameImageProvider& image_provider)
    : device_(util::not_null{&device}), frame_image_provider_(util::not_null{&image_provider}) {}

RenderTarget::~RenderTarget() {
    frame_image_provider_->removeRenderTarget(this);
    destroyFrameResources();
    for (std::uint32_t n = 0; n < std::uint32_t(frame_render_kits_.size()); ++n) {
        auto& kit = frame_render_kits_[n];
        device_->vkDestroyFence(kit.fence, nullptr);
        device_->getGraphicsQueue().releaseCommandBuffer(n, kit.command_buffer);
    }
    device_->vkDestroyRenderPass(render_pass_, nullptr);
}

bool RenderTarget::create(const uxs::db::value& opts) {
    use_depth_ = opts.value<bool>("use_depth");

    const std::uint32_t fif_count = frame_image_provider_->getFifCount();

    frame_render_kits_.resize(fif_count);
    device_->getGraphicsQueue().growCommandPoolCount(fif_count);
    for (std::uint32_t n = 0; n < fif_count; ++n) {
        auto& kit = frame_render_kits_[n];
        if (!device_->createFence(true, kit.fence)) { return false; }
        if (!device_->getGraphicsQueue().obtainCommandBuffer(n, kit.command_buffer)) { return false; }
    }

    image_format_ = frame_image_provider_->getImageFormat();

    uxs::inline_dynarray<VkAttachmentDescription, 2> attachments_descriptions;

    attachments_descriptions.emplace_back(VkAttachmentDescription{
        .format = image_format_,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = frame_image_provider_->getImageLayout(),
    });

    if (use_depth_) {
        attachments_descriptions.emplace_back(VkAttachmentDescription{
            .format = depth_stencil_format_,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        });
    }

    const std::array color_attachments{VkAttachmentReference{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    }};

    const VkAttachmentReference depth_stencil_attachment{
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    const std::array subpass_descriptions{
        Wrapper<VkSubpassDescription>::unwrap({
            .pipeline_type = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .color_attachments = color_attachments,
            .depth_stencil_attachment = use_depth_ ? &depth_stencil_attachment : nullptr,
        }),
    };

    const std::array subpass_dependencies{
        VkSubpassDependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_NONE,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        },
        VkSubpassDependency{
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = frame_image_provider_->getImageConsumingStages(),
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = frame_image_provider_->getImageAccess(),
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        },
    };

    const VkRenderPassCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = std::uint32_t(attachments_descriptions.size()),
        .pAttachments = attachments_descriptions.data(),
        .subpassCount = std::uint32_t(subpass_descriptions.size()),
        .pSubpasses = subpass_descriptions.data(),
        .dependencyCount = std::uint32_t(subpass_dependencies.size()),
        .pDependencies = subpass_dependencies.data(),
    };

    VkResult result = device_->vkCreateRenderPass(&create_info, nullptr, &render_pass_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create render pass: {}", result);
        return false;
    }

    return createFrameResources();
}

bool RenderTarget::createFrameResources() {
    const auto image_extent = frame_image_provider_->getImageExtent();
    image_extent_.width = image_extent.width;
    image_extent_.height = image_extent.height;

    for (auto& kit : frame_render_kits_) {
        uxs::inline_dynarray<VkFramebufferAttachmentImageInfo, 2> attachment_infos;

        attachment_infos.emplace_back(VkFramebufferAttachmentImageInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
            .usage = frame_image_provider_->getImageUsage(),
            .width = image_extent.width,
            .height = image_extent.height,
            .layerCount = 1,
            .viewFormatCount = 1,
            .pViewFormats = &image_format_,
        });

        if (use_depth_) {
            const VkImageCreateInfo create_info{
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = depth_stencil_format_,
                .extent = {.width = image_extent.width, .height = image_extent.height, .depth = 1},
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            };

            VkResult result = vmaCreateImage(device_->getAllocator(), &create_info,
                                             constAddressOf(VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_AUTO}),
                                             &kit.depth_stencil_image, &kit.depth_stencil_allocation, nullptr);
            if (result != VK_SUCCESS) {
                logError(LOG_VK "couldn't create depth&stencil image: {}", result);
                return false;
            }

            const VkImageViewCreateInfo view_create_info{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = kit.depth_stencil_image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = depth_stencil_format_,
                .subresourceRange =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                        .baseMipLevel = 0,
                        .levelCount = VK_REMAINING_MIP_LEVELS,
                        .baseArrayLayer = 0,
                        .layerCount = VK_REMAINING_ARRAY_LAYERS,
                    },
            };

            result = device_->vkCreateImageView(&view_create_info, nullptr, &kit.depth_stencil_image_view);
            if (result != VK_SUCCESS) {
                logError(LOG_VK "couldn't create depth&stencil image view: {}", result);
                return false;
            }

            attachment_infos.emplace_back(VkFramebufferAttachmentImageInfo{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .width = image_extent.width,
                .height = image_extent.height,
                .layerCount = 1,
                .viewFormatCount = 1,
                .pViewFormats = &depth_stencil_format_,
            });
        }

        const VkFramebufferAttachmentsCreateInfo attachments_create_info{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO,
            .attachmentImageInfoCount = std::uint32_t(attachment_infos.size()),
            .pAttachmentImageInfos = attachment_infos.data(),
        };

        const VkFramebufferCreateInfo framebuffer_create_info{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = &attachments_create_info,
            .flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,
            .renderPass = render_pass_,
            .attachmentCount = std::uint32_t(attachment_infos.size()),
            .width = image_extent.width,
            .height = image_extent.height,
            .layers = 1,
        };

        VkResult result = device_->vkCreateFramebuffer(&framebuffer_create_info, nullptr, &kit.framebuffer);
        if (result != VK_SUCCESS) {
            logError(LOG_VK "couldn't create framebuffer: {}", result);
            return false;
        }
    }

    n_frame_ = 0;
    render_target_status_ = RenderTargetResult::SUCCESS;
    return true;
}

void RenderTarget::destroyFrameResources() {
    for (auto& kit : frame_render_kits_) {
        device_->waitForFences(std::array{kit.fence}, VK_FALSE, FINISH_FRAME_TIMEOUT);
        device_->vkDestroyFramebuffer(kit.framebuffer, nullptr);
        device_->vkDestroyImageView(kit.depth_stencil_image_view, nullptr);
        vmaDestroyImage(device_->getAllocator(), kit.depth_stencil_image, kit.depth_stencil_allocation);
        kit.framebuffer = VK_NULL_HANDLE;
        kit.depth_stencil_image_view = VK_NULL_HANDLE;
        kit.depth_stencil_image = VK_NULL_HANDLE;
        kit.depth_stencil_allocation = VK_NULL_HANDLE;
    }
}

//@{ IRenderTarget

RenderTargetResult RenderTarget::beginRenderTarget(const Color4f& clear_color, float depth, std::uint32_t stencil,
                                                   IPipeline& pipeline) {
    if (render_target_status_ > RenderTargetResult::SUBOPTIMAL) { return render_target_status_; }

    auto& kit = frame_render_kits_[n_frame_];

    if (!device_->waitForFences(std::array{kit.fence}, VK_FALSE, FINISH_FRAME_TIMEOUT)) {
        return RenderTargetResult::FAILED;
    }

    if (!device_->getGraphicsQueue().resetCommandPool(n_frame_)) { return RenderTargetResult::FAILED; }

    current_image_index_ = 0;
    render_target_status_ = frame_image_provider_->acquireFrameImage(ACQUIRE_FRAME_IMAGE_TIMEOUT, current_image_index_);
    if (render_target_status_ > RenderTargetResult::SUBOPTIMAL) { return render_target_status_; }

    if (!kit.command_buffer.beginCommandBuffer(0, nullptr)) { return RenderTargetResult::FAILED; }

    frame_image_provider_->imageBarrierBefore(kit.command_buffer, current_image_index_);

    uxs::inline_dynarray<VkClearValue, 2> clear_values;
    uxs::inline_dynarray<VkImageView, 2> attachments;

    clear_values.emplace_back(VkClearValue{.color = {{clear_color.r, clear_color.g, clear_color.b, clear_color.a}}});
    attachments.push_back(frame_image_provider_->getImageView(current_image_index_));
    if (use_depth_) {
        clear_values.emplace_back(VkClearValue{.depthStencil = {depth, stencil}});
        attachments.push_back(kit.depth_stencil_image_view);
    }

    const VkRect2D view_rect{.offset = {.x = 0, .y = 0}, .extent = image_extent_};

    kit.command_buffer.beginRenderPass(render_pass_, kit.framebuffer, view_rect, VK_SUBPASS_CONTENTS_INLINE,
                                       clear_values, attachments);

    kit.command_buffer.setViewports(0, std::array{VkViewport{.x = 0.f,
                                                             .y = 0.f,
                                                             .width = float(image_extent_.width),
                                                             .height = float(image_extent_.height),
                                                             .minDepth = 0.f,
                                                             .maxDepth = 1.f}});

    kit.command_buffer.setScissors(0, std::array{view_rect});

    current_pipeline_ = &static_cast<Pipeline&>(pipeline);
    kit.command_buffer.vkCmdBindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, current_pipeline_->getHandle());

    return render_target_status_;
}

bool RenderTarget::endRenderTarget() {
    auto& kit = frame_render_kits_[n_frame_];

    kit.command_buffer.vkCmdEndRenderPass();

    frame_image_provider_->imageBarrierAfter(kit.command_buffer, current_image_index_);

    if (!kit.command_buffer.endCommandBuffer()) { return false; }

    if (!device_->resetFences(std::array{kit.fence})) { return false; }

    render_target_status_ = frame_image_provider_->submitFrameImage(current_image_index_, kit.command_buffer, kit.fence);

    if (++n_frame_ == frame_render_kits_.size()) { n_frame_ = 0; }
    return render_target_status_ <= RenderTargetResult::OUT_OF_DATE;
}

void RenderTarget::setViewport(const Rect& rect, float z_near, float z_far) {
    auto& kit = frame_render_kits_[n_frame_];
    kit.command_buffer.setViewports(0, std::array{VkViewport{
                                           .x = float(rect.offset.x),
                                           .y = float(rect.offset.y),
                                           .width = float(rect.extent.width),
                                           .height = float(rect.extent.height),
                                           .minDepth = z_near,
                                           .maxDepth = z_far,
                                       }});
}

void RenderTarget::setScissor(const Rect& rect) {
    auto& kit = frame_render_kits_[n_frame_];
    kit.command_buffer.setScissors(
        0, std::array{VkRect2D{.offset = {.x = rect.offset.x, .y = rect.offset.y},
                               .extent = {.width = rect.extent.width, .height = rect.extent.height}}});
}

void RenderTarget::bindPipeline(IPipeline& pipeline) {
    auto& kit = frame_render_kits_[n_frame_];
    current_pipeline_ = &static_cast<Pipeline&>(pipeline);
    kit.command_buffer.vkCmdBindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, current_pipeline_->getHandle());
}

void RenderTarget::bindVertexBuffer(IBuffer& buffer, std::uint32_t slot, std::uint32_t stride, std::uint32_t offset) {
    auto& kit = frame_render_kits_[n_frame_];
    if (stride != 0) {
        kit.command_buffer.bindVertexBuffers2(
            slot, {std::array{static_cast<Buffer&>(buffer).getHandle()}, std::array{VkDeviceSize(offset)},
                   std::array{static_cast<Buffer&>(buffer).getSize() - offset}, std::array{VkDeviceSize(stride)}});
    } else {
        kit.command_buffer.bindVertexBuffers(
            slot, {std::array{static_cast<Buffer&>(buffer).getHandle()}, std::array{VkDeviceSize(offset)}});
    }
}

void RenderTarget::bindDescriptorSet(IDescriptorSet& descriptor_set, std::uint32_t set_index) {
    auto& kit = frame_render_kits_[n_frame_];
    kit.command_buffer.bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, current_pipeline_->getLayout().getHandle(),
                                          set_index,
                                          std::array{static_cast<DescriptorSet&>(descriptor_set).getHandle()}, {});
}

void RenderTarget::bindDescriptorSetDynamic(IDescriptorSet& descriptor_set, std::uint32_t set_index,
                                            std::span<const std::uint32_t> offsets) {
    auto& kit = frame_render_kits_[n_frame_];
    kit.command_buffer.bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, current_pipeline_->getLayout().getHandle(),
                                          set_index,
                                          std::array{static_cast<DescriptorSet&>(descriptor_set).getHandle()}, offsets);
}

void RenderTarget::setPrimitiveTopology(PrimitiveTopology topology) {
    auto& kit = frame_render_kits_[n_frame_];
    kit.command_buffer.vkCmdSetPrimitiveTopologyEXT(TBL_VK_PRIMITIVE_TOPOLOGY[unsigned(topology)]);
}

void RenderTarget::drawGeometry(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex,
                                std::uint32_t first_instance) {
    auto& kit = frame_render_kits_[n_frame_];
    kit.command_buffer.vkCmdDraw(vertex_count, instance_count, first_vertex, first_instance);
}

//@}
