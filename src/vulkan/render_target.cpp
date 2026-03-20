#include "render_target.h"

#include "descriptor_set.h"
#include "device.h"
#include "object_destroyer.h"
#include "pipeline.h"
#include "surface.h"
#include "swap_chain.h"
#include "vulkan_logger.h"
#include "wrappers.h"

#include <array>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// RenderTarget class implementation

RenderTarget::RenderTarget(Device& device, SwapChain& swap_chain) : device_(device), swap_chain_(swap_chain) {}

RenderTarget::~RenderTarget() {
    destroyImageViews();
    device_.getGraphicsQueue().releaseCommandBuffer(~command_buffer_);
    ObjectDestroyer<VkRenderPass>::destroy(~device_, render_pass_);
    ObjectDestroyer<VkFence>::destroy(~device_, fence_drawing_);
    ObjectDestroyer<VkSemaphore>::destroy(~device_, sem_image_acquired_);
    for (const auto& sem : sem_ready_to_present_) { ObjectDestroyer<VkSemaphore>::destroy(~device_, sem); }
}

bool RenderTarget::create(const uxs::db::value& opts) {
    // Drawing synchronization

    if (!device_.createSemaphore(sem_image_acquired_)) { return false; }
    if (!device_.createFence(true, fence_drawing_)) { return false; }

    sem_ready_to_present_.resize(3);
    for (auto& sem : sem_ready_to_present_) {
        if (!device_.createSemaphore(sem)) { return false; }
    }

    // Create render pass

    const std::array attachments_descriptions{
        VkAttachmentDescription{
            .format = swap_chain_.getSurface().getImageFormat().format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        },
    };

    const std::array color_attachments{VkAttachmentReference{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    }};

    const std::array subpass_descriptions{
        Wrapper<VkSubpassDescription>::unwrap({
            .pipeline_type = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .color_attachments = color_attachments,
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
            .dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
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

    VkResult result = vkCreateRenderPass(~device_, &create_info, nullptr, &render_pass_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create render pass: {}", result);
        return false;
    }

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    if (!device_.getGraphicsQueue().obtainCommandBuffer(command_buffer)) { return false; }
    command_buffer_ = CommandBuffer::wrap(command_buffer);

    return true;
}

bool RenderTarget::createImageViews() {
    image_views_.resize(swap_chain_.getImageCount());

    for (std::size_t n = 0; n < image_views_.size(); ++n) {
        const VkImageViewCreateInfo image_view_create_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swap_chain_.getImage(n),
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swap_chain_.getSurface().getImageFormat().format,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };

        VkResult result = vkCreateImageView(~device_, &image_view_create_info, nullptr, &image_views_[n].image_view);
        if (result != VK_SUCCESS) {
            logError(LOG_VK "couldn't create image view for swap chain image: {}", result);
            return false;
        }

        const std::array attachments{image_views_[n].image_view};
        const auto& extent = swap_chain_.getImageExtent();

        const VkFramebufferCreateInfo framebuffer_create_info{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass_,
            .attachmentCount = std::uint32_t(attachments.size()),
            .pAttachments = attachments.data(),
            .width = extent.width,
            .height = extent.height,
            .layers = 1,
        };

        result = vkCreateFramebuffer(~device_, &framebuffer_create_info, nullptr, &image_views_[n].framebuffer);
        if (result != VK_SUCCESS) {
            logError(LOG_VK "couldn't create framebuffer: {}", result);
            return false;
        }
    }

    render_target_status_ = RenderTargetResult::SUCCESS;
    return true;
}

void RenderTarget::destroyImageViews() {
    for (const auto& item : image_views_) {
        ObjectDestroyer<VkFramebuffer>::destroy(~device_, item.framebuffer);
        ObjectDestroyer<VkImageView>::destroy(~device_, item.image_view);
    }
    image_views_.clear();
}

//@{ IRenderTarget

RenderTargetResult RenderTarget::beginRenderTarget(const Color4f& clear_color) {
    if (render_target_status_ > RenderTargetResult::SUBOPTIMAL) { return render_target_status_; }

    if (!device_.waitForFences(std::array{fence_drawing_}, false, 5000000000)) { return RenderTargetResult::FAILED; }

    std::uint32_t image_index = 0;
    render_target_status_ = swap_chain_.acquireImage(2000000000, sem_image_acquired_, VK_NULL_HANDLE, image_index);
    if (render_target_status_ > RenderTargetResult::SUBOPTIMAL) { return render_target_status_; }

    auto& graphics_queue = device_.getGraphicsQueue();
    auto& present_queue = device_.getPresentQueue();

    if (!command_buffer_.beginCommandBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr)) {
        return RenderTargetResult::FAILED;
    }

    const auto& image_extent = swap_chain_.getImageExtent();
    command_buffer_.beginRenderPass(
        render_pass_, image_views_[image_index].framebuffer,
        VkRect2D{.extent = {.width = image_extent.width, .height = image_extent.height}},
        std::array{VkClearValue{.color = {{clear_color.r, clear_color.g, clear_color.b, clear_color.a}}}},
        VK_SUBPASS_CONTENTS_INLINE);

    current_image_index_ = image_index;
    return render_target_status_;
}

bool RenderTarget::endRenderTarget() {
    auto& graphics_queue = device_.getGraphicsQueue();
    auto& present_queue = device_.getPresentQueue();

    if (current_image_index_ == INVALID_UINT32_VALUE) { return false; }
    const std::uint32_t image_index = current_image_index_;
    current_image_index_ = INVALID_UINT32_VALUE;

    command_buffer_.endRenderPass();

    if (!command_buffer_.endCommandBuffer()) { return false; }

    if (!device_.resetFences(std::array{fence_drawing_})) { return false; }

    if (++n_frame_ == sem_ready_to_present_.size()) { n_frame_ = 0; }

    if (!graphics_queue.submitCommandBuffers(
            {std::array{sem_image_acquired_}, std::array{VkPipelineStageFlags(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)}},
            std::array{~command_buffer_}, std::array{sem_ready_to_present_[n_frame_]}, fence_drawing_)) {
        return false;
    }

    render_target_status_ = present_queue.presentImages(std::array{sem_ready_to_present_[n_frame_]},
                                                        {std::array{~swap_chain_}, std::array{image_index}});

    return render_target_status_ <= RenderTargetResult::OUT_OF_DATE;
}

void RenderTarget::setViewport(const Rect& rect, float z_near, float z_far) {
    command_buffer_.setViewportState(0, std::array{VkViewport{
                                            .x = float(rect.offset.x),
                                            .y = float(rect.offset.y),
                                            .width = float(rect.extent.width),
                                            .height = float(rect.extent.height),
                                            .minDepth = z_near,
                                            .maxDepth = z_far,
                                        }});
}

void RenderTarget::setScissor(const Rect& rect) {
    command_buffer_.setScissorState(
        0, std::array{VkRect2D{.offset = {.x = rect.offset.x, .y = rect.offset.y},
                               .extent = {.width = rect.extent.width, .height = rect.extent.height}}});
}

void RenderTarget::bindPipeline(IPipeline& pipeline) {
    command_buffer_.bindPipelineObject(VK_PIPELINE_BIND_POINT_GRAPHICS, ~static_cast<Pipeline&>(pipeline));
}

void RenderTarget::bindVertexBuffer(IBuffer& buffer, std::size_t offset, std::uint32_t binding) {
    command_buffer_.bindVertexBuffers(binding, {std::array{~static_cast<Buffer&>(buffer)}, std::array{offset}});
}

void RenderTarget::bindDescriptorSet(IPipeline& pipeline, IDescriptorSet& descriptor_set, std::uint32_t set_index) {
    command_buffer_.bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, static_cast<Pipeline&>(pipeline).getLayout(),
                                       set_index, std::array{~static_cast<DescriptorSet&>(descriptor_set)}, {});
}

void RenderTarget::drawGeometry(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex,
                                std::uint32_t first_instance) {
    command_buffer_.drawGeometry(vertex_count, instance_count, first_vertex, first_instance);
}

//@}
