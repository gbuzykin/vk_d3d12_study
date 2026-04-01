#include "render_target.h"

#include "descriptor_set.h"
#include "device.h"
#include "object_destroyer.h"
#include "pipeline.h"
#include "pipeline_layout.h"
#include "surface.h"
#include "swap_chain.h"
#include "vulkan_logger.h"
#include "wrappers.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// RenderTarget class implementation

RenderTarget::RenderTarget(Device& device, SwapChain& swap_chain) : device_(device), swap_chain_(swap_chain) {}

RenderTarget::~RenderTarget() {
    destroyFrameResources();
    for (auto& kit : frame_render_kits_) {
        device_.getGraphicsQueue().releaseCommandBuffer(~kit.command_buffer);
        ObjectDestroyer<VkFence>::destroy(~device_, kit.fence_drawing);
        ObjectDestroyer<VkSemaphore>::destroy(~device_, kit.sem_image_acquired);
        ObjectDestroyer<VkSemaphore>::destroy(~device_, kit.sem_ready_to_present);
    }
    frame_render_kits_.clear();
    ObjectDestroyer<VkRenderPass>::destroy(~device_, render_pass_);
}

bool RenderTarget::create(const uxs::db::value& opts) {
    use_depth_ = opts.value<bool>("use_depth");

    uxs::inline_dynarray<VkAttachmentDescription, 2> attachments_descriptions;

    attachments_descriptions.push_back(VkAttachmentDescription{
        .format = swap_chain_.getSurface().getImageFormat().format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
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

    const VkAttachmentReference depth_attachment{
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    const std::array subpass_descriptions{
        Wrapper<VkSubpassDescription>::unwrap({
            .pipeline_type = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .color_attachments =
                std::array{
                    VkAttachmentReference{
                        .attachment = 0,
                        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    },
                },
            .depth_stencil_attachment = use_depth_ ? &depth_attachment : nullptr,
        }),
    };

    const std::array subpass_dependencies{
        VkSubpassDependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
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

    frame_render_kits_.resize(MAX_FIF_COUNT);
    for (auto& kit : frame_render_kits_) {
        if (!device_.createSemaphore(kit.sem_image_acquired)) { return false; }
        if (!device_.createSemaphore(kit.sem_ready_to_present)) { return false; }
        if (!device_.createFence(true, kit.fence_drawing)) { return false; }
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        if (!device_.getGraphicsQueue().obtainCommandBuffer(command_buffer)) { return false; }
        kit.command_buffer = CommandBuffer::wrap(command_buffer);
    }

    return true;
}

bool RenderTarget::createFrameResources() {
    const auto& extent = swap_chain_.getImageExtent();

    frame_resources_.resize(swap_chain_.getImageCount());

    for (unsigned n = 0; n < swap_chain_.getImageCount(); ++n) {
        auto& res = frame_resources_[n];

        uxs::inline_dynarray<VkImageView, 2> attachments;
        attachments.push_back(swap_chain_.getImageView(n));

        if (use_depth_) {
            const VkImageCreateInfo create_info{
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = depth_format_,
                .extent = {.width = extent.width, .height = extent.height, .depth = 1},
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            };

            const VmaAllocationCreateInfo alloc_info{.usage = VMA_MEMORY_USAGE_AUTO};

            VkResult result = vmaCreateImage(device_.getAllocator(), &create_info, &alloc_info, &res.depth_image,
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

            result = vkCreateImageView(~device_, &view_create_info, nullptr, &res.depth_image_view);
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
            .width = extent.width,
            .height = extent.height,
            .layers = 1,
        };

        VkResult result = vkCreateFramebuffer(~device_, &framebuffer_create_info, nullptr,
                                              &frame_resources_[n].framebuffer);
        if (result != VK_SUCCESS) {
            logError(LOG_VK "couldn't create framebuffer: {}", result);
            return false;
        }
    }

    swap_chain_status_ = RenderTargetResult::SUCCESS;
    return true;
}

void RenderTarget::destroyFrameResources() {
    for (const auto& item : frame_resources_) {
        ObjectDestroyer<VkFramebuffer>::destroy(~device_, item.framebuffer);
        ObjectDestroyer<VkImageView>::destroy(~device_, item.depth_image_view);
        vmaDestroyImage(device_.getAllocator(), item.depth_image, item.depth_allocation);
    }
    frame_resources_.clear();
}

//@{ IRenderTarget

RenderTargetResult RenderTarget::beginRenderTarget(const Color4f& clear_color, float depth, std::uint32_t stencil) {
    if (swap_chain_status_ > RenderTargetResult::SUBOPTIMAL) { return swap_chain_status_; }

    auto& kit = frame_render_kits_[n_frame_];

    if (!device_.waitForFences(std::array{kit.fence_drawing}, false, 5000000000)) { return RenderTargetResult::FAILED; }

    std::uint32_t image_index = 0;
    swap_chain_status_ = swap_chain_.acquireImage(2000000000, kit.sem_image_acquired, VK_NULL_HANDLE, image_index);
    if (swap_chain_status_ > RenderTargetResult::SUBOPTIMAL) { return swap_chain_status_; }

    auto& graphics_queue = device_.getGraphicsQueue();
    auto& present_queue = device_.getPresentQueue();

    if (!kit.command_buffer.beginCommandBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr)) {
        return RenderTargetResult::FAILED;
    }

    if (present_queue.getFamilyIndex() != graphics_queue.getFamilyIndex()) {
        kit.command_buffer.setImageMemoryBarrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                 std::array{
                                                     Wrapper<VkImageMemoryBarrier>::unwrap({
                                                         .image = swap_chain_.getImage(image_index),
                                                         .current_access = VK_ACCESS_MEMORY_READ_BIT,
                                                         .new_access = VK_ACCESS_MEMORY_READ_BIT,
                                                         .current_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                         .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                         .current_queue_family = present_queue.getFamilyIndex(),
                                                         .new_queue_family = graphics_queue.getFamilyIndex(),
                                                         .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                                                     }),
                                                 });
    }

    uxs::inline_dynarray<VkClearValue, 2> clear_values;
    clear_values.push_back(VkClearValue{.color = {{clear_color.r, clear_color.g, clear_color.b, clear_color.a}}});
    if (use_depth_) { clear_values.push_back(VkClearValue{.depthStencil = {depth, stencil}}); }

    const auto& image_extent = swap_chain_.getImageExtent();
    kit.command_buffer.beginRenderPass(render_pass_, frame_resources_[image_index].framebuffer,
                                       VkRect2D{.extent = {.width = image_extent.width, .height = image_extent.height}},
                                       clear_values, VK_SUBPASS_CONTENTS_INLINE);

    current_image_index_ = image_index;
    current_pipeline_ = nullptr;
    return swap_chain_status_;
}

bool RenderTarget::endRenderTarget() {
    auto& graphics_queue = device_.getGraphicsQueue();
    auto& present_queue = device_.getPresentQueue();

    const std::uint32_t image_index = current_image_index_;

    auto& kit = frame_render_kits_[n_frame_++];
    if (n_frame_ == frame_render_kits_.size()) { n_frame_ = 0; }

    kit.command_buffer.endRenderPass();

    if (present_queue.getFamilyIndex() != graphics_queue.getFamilyIndex()) {
        kit.command_buffer.setImageMemoryBarrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                                 std::array{
                                                     Wrapper<VkImageMemoryBarrier>::unwrap({
                                                         .image = swap_chain_.getImage(image_index),
                                                         .current_access = VK_ACCESS_MEMORY_READ_BIT,
                                                         .new_access = VK_ACCESS_MEMORY_READ_BIT,
                                                         .current_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                         .new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                         .current_queue_family = graphics_queue.getFamilyIndex(),
                                                         .new_queue_family = present_queue.getFamilyIndex(),
                                                         .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                                                     }),
                                                 });
    }

    if (!kit.command_buffer.endCommandBuffer()) { return false; }

    if (!device_.resetFences(std::array{kit.fence_drawing})) { return false; }

    if (!graphics_queue.submitCommandBuffers(
            {std::array{kit.sem_image_acquired}, std::array{VkPipelineStageFlags(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)}},
            std::array{~kit.command_buffer}, std::array{kit.sem_ready_to_present}, kit.fence_drawing)) {
        return false;
    }

    swap_chain_status_ = present_queue.presentImages(std::array{kit.sem_ready_to_present},
                                                     {std::array{~swap_chain_}, std::array{image_index}});
    return swap_chain_status_ <= RenderTargetResult::OUT_OF_DATE;
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
