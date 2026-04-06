#include "render_target.h"

#include "descriptor_set.h"
#include "device.h"
#include "object_destroyer.h"
#include "pipeline.h"
#include "pipeline_layout.h"
#include "swap_chain.h"
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
    for (auto& kit : frame_render_kits_) {
        ObjectDestroyer<VkFence>::destroy(~*device_, kit.fence);
        device_->getGraphicsQueue().releaseCommandBuffer(~kit.command_buffer);
    }
    ObjectDestroyer<VkRenderPass>::destroy(~*device_, render_pass_);
}

bool RenderTarget::create(const uxs::db::value& opts) {
    use_depth_ = opts.value<bool>("use_depth");
    frame_render_kits_.resize(frame_image_provider_->getFifCount());
    for (auto& kit : frame_render_kits_) {
        if (!device_->createFence(true, kit.fence)) { return false; }
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        if (!device_->getGraphicsQueue().obtainCommandBuffer(command_buffer)) { return false; }
        kit.command_buffer = CommandBuffer::wrap(command_buffer);
    }

    uxs::inline_dynarray<VkAttachmentDescription, 2> attachments_descriptions;

    attachments_descriptions.push_back(VkAttachmentDescription{
        .format = frame_image_provider_->getImageFormat(),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = frame_image_provider_->getImageLayout(),
    });

    if (use_depth_) {
        attachments_descriptions.push_back(VkAttachmentDescription{
            .format = depth_format_,
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

    const VkAttachmentReference depth_attachment{
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    const std::array subpass_descriptions{
        Wrapper<VkSubpassDescription>::unwrap({
            .pipeline_type = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .color_attachments = color_attachments,
            .depth_stencil_attachment = use_depth_ ? &depth_attachment : nullptr,
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

    VkResult result = vkCreateRenderPass(~*device_, &create_info, nullptr, &render_pass_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create render pass: {}", result);
        return false;
    }

    return createFrameResources();
}

bool RenderTarget::createFrameResources() {
    image_extent_ = frame_image_provider_->getImageExtent();

    const std::uint32_t image_count = frame_image_provider_->getImageCount();

    frame_resources_.resize(image_count);

    for (std::uint32_t n = 0; n < image_count; ++n) {
        auto& res = frame_resources_[n];

        uxs::inline_dynarray<VkImageView, 2> attachments;
        attachments.push_back(frame_image_provider_->getImageView(n));

        if (use_depth_) {
            const VkImageCreateInfo create_info{
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = depth_format_,
                .extent = {.width = image_extent_.width, .height = image_extent_.height, .depth = 1},
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            };

            const VmaAllocationCreateInfo alloc_info{.usage = VMA_MEMORY_USAGE_AUTO};

            VkResult result = vmaCreateImage(device_->getAllocator(), &create_info, &alloc_info, &res.depth_image,
                                             &res.depth_allocation, nullptr);
            if (result != VK_SUCCESS) {
                logError(LOG_VK "couldn't create depth buffer: {}", result);
                return false;
            }

            const VkImageViewCreateInfo view_create_info{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = res.depth_image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = depth_format_,
                .subresourceRange =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                        .baseMipLevel = 0,
                        .levelCount = VK_REMAINING_MIP_LEVELS,
                        .baseArrayLayer = 0,
                        .layerCount = VK_REMAINING_ARRAY_LAYERS,
                    },
            };

            result = vkCreateImageView(~*device_, &view_create_info, nullptr, &res.depth_image_view);
            if (result != VK_SUCCESS) {
                logError(LOG_VK "couldn't create depth buffer view: {}", result);
                return false;
            }

            attachments.push_back(res.depth_image_view);
        }

        const VkFramebufferCreateInfo framebuffer_create_info{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass_,
            .attachmentCount = std::uint32_t(attachments.size()),
            .pAttachments = attachments.data(),
            .width = image_extent_.width,
            .height = image_extent_.height,
            .layers = 1,
        };

        VkResult result = vkCreateFramebuffer(~*device_, &framebuffer_create_info, nullptr,
                                              &frame_resources_[n].framebuffer);
        if (result != VK_SUCCESS) {
            logError(LOG_VK "couldn't create framebuffer: {}", result);
            return false;
        }
    }

    render_target_status_ = RenderTargetResult::SUCCESS;
    return true;
}

void RenderTarget::destroyFrameResources() {
    for (const auto& kit : frame_render_kits_) {
        device_->waitForFences(std::array{kit.fence}, VK_FALSE, FINISH_FRAME_TIMEOUT);
    }
    for (const auto& item : frame_resources_) {
        ObjectDestroyer<VkFramebuffer>::destroy(~*device_, item.framebuffer);
        ObjectDestroyer<VkImageView>::destroy(~*device_, item.depth_image_view);
        vmaDestroyImage(device_->getAllocator(), item.depth_image, item.depth_allocation);
    }
    frame_resources_.clear();
}

//@{ IRenderTarget

RenderTargetResult RenderTarget::beginRenderTarget(const Color4f& clear_color, float depth, std::uint32_t stencil) {
    if (render_target_status_ > RenderTargetResult::SUBOPTIMAL) { return render_target_status_; }

    if (++n_frame_ == frame_render_kits_.size()) { n_frame_ = 0; }
    auto& kit = frame_render_kits_[n_frame_];

    if (!device_->waitForFences(std::array{kit.fence}, VK_FALSE, FINISH_FRAME_TIMEOUT)) {
        return RenderTargetResult::FAILED;
    }

    std::uint32_t image_index = 0;
    render_target_status_ = frame_image_provider_->acquireFrameImage(n_frame_, ACQUIRE_FRAME_IMAGE_TIMEOUT, image_index);
    if (render_target_status_ > RenderTargetResult::SUBOPTIMAL) { return render_target_status_; }

    if (!kit.command_buffer.beginCommandBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr)) {
        return RenderTargetResult::FAILED;
    }

    frame_image_provider_->imageBarrierBefore(kit.command_buffer, image_index);

    uxs::inline_dynarray<VkClearValue, 2> clear_values;
    clear_values.push_back(VkClearValue{.color = {{clear_color.r, clear_color.g, clear_color.b, clear_color.a}}});
    if (use_depth_) { clear_values.push_back(VkClearValue{.depthStencil = {depth, stencil}}); }

    kit.command_buffer.beginRenderPass(
        render_pass_, frame_resources_[image_index].framebuffer,
        VkRect2D{.extent = {.width = image_extent_.width, .height = image_extent_.height}}, clear_values,
        VK_SUBPASS_CONTENTS_INLINE);

    current_pipeline_ = nullptr;
    current_image_index_ = image_index;
    return render_target_status_;
}

bool RenderTarget::endRenderTarget() {
    const std::uint32_t image_index = current_image_index_;

    auto& kit = frame_render_kits_[n_frame_];

    kit.command_buffer.endRenderPass();

    frame_image_provider_->imageBarrierAfter(kit.command_buffer, image_index);

    if (!kit.command_buffer.endCommandBuffer()) { return false; }

    if (!device_->resetFences(std::array{kit.fence})) { return false; }

    render_target_status_ = frame_image_provider_->submitFrameImage(n_frame_, image_index, kit.command_buffer,
                                                                    kit.fence);

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
    kit.command_buffer.bindPipelineObject(VK_PIPELINE_BIND_POINT_GRAPHICS, ~*current_pipeline_);
}

void RenderTarget::bindVertexBuffer(IBuffer& buffer, std::size_t offset, std::uint32_t slot) {
    auto& kit = frame_render_kits_[n_frame_];
    kit.command_buffer.bindVertexBuffers(current_pipeline_->getBinding(Pipeline::BindingType::VERTEX_BUFFER, slot),
                                         {std::array{~static_cast<Buffer&>(buffer)}, std::array{offset}});
}

void RenderTarget::bindDescriptorSet(IDescriptorSet& descriptor_set, std::uint32_t set_index) {
    auto& kit = frame_render_kits_[n_frame_];
    kit.command_buffer.bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS,
                                          ~static_cast<DescriptorSet&>(descriptor_set).getLayout(), set_index,
                                          std::array{~static_cast<DescriptorSet&>(descriptor_set)}, {});
}

void RenderTarget::setPrimitiveTopology(PrimitiveTopology topology) {
    constexpr std::array topologies{
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST,    VK_PRIMITIVE_TOPOLOGY_LINE_LIST,      VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    };
    auto& kit = frame_render_kits_[n_frame_];
    kit.command_buffer.setPrimitiveTopology(topologies[unsigned(topology)]);
}

void RenderTarget::drawGeometry(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex,
                                std::uint32_t first_instance) {
    auto& kit = frame_render_kits_[n_frame_];
    kit.command_buffer.drawGeometry(vertex_count, instance_count, first_vertex, first_instance);
}

//@}
