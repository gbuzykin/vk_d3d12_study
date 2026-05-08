#include "render_target.h"

#include "buffer.h"
#include "d3d12_logger.h"
#include "descriptor_set.h"
#include "device.h"
#include "pipeline.h"
#include "swap_chain.h"
#include "tables.h"
#include "wrappers.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// RenderTarget class implementation

RenderTarget::RenderTarget(Device& device, FrameImageProvider& image_provider)
    : device_(util::not_null{&device}), frame_image_provider_(util::not_null{&image_provider}) {}

RenderTarget::~RenderTarget() {
    destroyFrameResources();
    frame_image_provider_->removeRenderTarget(this);
}

bool RenderTarget::create(const uxs::db::value& opts) {
    use_depth_ = opts.value<bool>("use_depth");
    image_count_ = frame_image_provider_->getImageCount();

    const std::uint32_t fif_count = frame_image_provider_->getFifCount();

    frame_render_kits_.resize(fif_count);
    device_->getDirectQueue().growAllocatorCount(fif_count);
    for (std::uint32_t n = 0; n < fif_count; ++n) {
        if (!frame_render_kits_[n].fence.create(*device_)) { return false; }
        if (!(frame_render_kits_[n].command_list = device_->getDirectQueue().createGraphicsCommandList(n))) {
            return false;
        }
    }

    const D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = UINT(image_count_),
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0,
    };

    HRESULT result = device_->getD3D12Device()->CreateDescriptorHeap(&rtv_heap_desc,
                                                                     IID_PPV_ARGS(rtv_heap_.reset_and_get_address()));
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create RTV heap: {}", D3D12Result(result));
        return false;
    }

    if (use_depth_) {
        const D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc{
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            .NumDescriptors = UINT(fif_count),
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0,
        };

        result = device_->getD3D12Device()->CreateDescriptorHeap(&dsv_heap_desc,
                                                                 IID_PPV_ARGS(dsv_heap_.reset_and_get_address()));
        if (result != S_OK) {
            logError(LOG_D3D12 "couldn't create DSV heap: {}", D3D12Result(result));
            return false;
        }
    }

    return createFrameResources();
}

bool RenderTarget::createFrameResources() {
    image_extent_ = frame_image_provider_->getImageExtent();
    image_format_ = TBL_D3D12_FORMAT[unsigned(frame_image_provider_->getImageFormat())];

    auto rtv_handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();

    for (std::uint32_t n = 0; n < image_count_; ++n) {
        device_->getD3D12Device()->CreateRenderTargetView(frame_image_provider_->getImageResource(n), nullptr,
                                                          rtv_handle);
        rtv_handle += device_->getRtvDescriptorSize();
    }

    if (use_depth_) {
        auto dsv_handle = dsv_heap_->GetCPUDescriptorHandleForHeapStart();

        for (std::uint32_t n = 0; n < std::uint32_t(frame_render_kits_.size()); ++n) {
            auto& kit = frame_render_kits_[n];

            const D3D12_RESOURCE_DESC depth_stencil_desc{
                .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                .Alignment = 0,
                .Width = UINT64(image_extent_.width),
                .Height = UINT64(image_extent_.height),
                .DepthOrArraySize = 1,
                .MipLevels = 1,
                .Format = DXGI_FORMAT_R24G8_TYPELESS,
                .SampleDesc = {.Count = 1, .Quality = 0},
                .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
                .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
            };

            HRESULT result = device_->getAllocator()->CreateResource(
                constAddressOf(D3D12MA::ALLOCATION_DESC{.HeapType = D3D12_HEAP_TYPE_DEFAULT}), &depth_stencil_desc,
                D3D12_RESOURCE_STATE_COMMON,
                constAddressOf(D3D12_CLEAR_VALUE{
                    .Format = depth_stencil_format_,
                    .DepthStencil = {.Depth = 1.f, .Stencil = 0},
                }),
                kit.depth_stencil_allocation.reset_and_get_address(), IID_NULL, nullptr);
            if (result != S_OK) {
                logError(LOG_D3D12 "couldn't create depth&stencil resource: {}", D3D12Result(result));
                return false;
            }

            device_->getD3D12Device()->CreateDepthStencilView(kit.depth_stencil_allocation->GetResource(),
                                                              constAddressOf(D3D12_DEPTH_STENCIL_VIEW_DESC{
                                                                  .Format = depth_stencil_format_,
                                                                  .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
                                                                  .Flags = D3D12_DSV_FLAG_NONE,
                                                              }),
                                                              dsv_handle);
            dsv_handle += device_->getDsvDescriptorSize();

            if (!device_->getDirectQueue().resetAllocator(n)) { return false; }

            if (!device_->getDirectQueue().resetCommandList(n, kit.command_list.get(), nullptr)) { return false; }

            kit.command_list->ResourceBarrier(1, constAddressOf(Wrapper<D3D12_RESOURCE_BARRIER>::transition({
                                                     .resource = kit.depth_stencil_allocation->GetResource(),
                                                     .state_before = D3D12_RESOURCE_STATE_COMMON,
                                                     .state_after = D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                                 })));

            if (!DevQueue::closeCommandList(kit.command_list.get())) { return false; }

            device_->getDirectQueue().executeCommandLists(
                std::array{static_cast<ID3D12CommandList*>(kit.command_list.get())});

            kit.fence.queueSignal(device_->getDirectQueue());
        }
    }

    n_frame_ = 0;
    current_image_index_ = 0;
    render_target_status_ = RenderTargetResult::SUCCESS;
    return true;
}

void RenderTarget::destroyFrameResources() {
    for (auto& kit : frame_render_kits_) {
        kit.fence.wait();
        kit.depth_stencil_allocation.reset();
    }
}

//@{ IRenderTarget

RenderTargetResult RenderTarget::beginRenderTarget(const Color4f& clear_color, float depth, std::uint32_t stencil,
                                                   IPipeline& pipeline) {
    if (render_target_status_ > RenderTargetResult::SUBOPTIMAL) { return render_target_status_; }

    auto& kit = frame_render_kits_[n_frame_];

    if (!kit.fence.wait()) { return RenderTargetResult::FAILED; }

    if (!device_->getDirectQueue().resetAllocator(n_frame_)) { return RenderTargetResult::FAILED; }

    current_pipeline_ = &static_cast<Pipeline&>(pipeline);
    if (!device_->getDirectQueue().resetCommandList(n_frame_, kit.command_list.get(),
                                                    current_pipeline_->getD3D12PipelineState())) {
        return RenderTargetResult::FAILED;
    }

    frame_image_provider_->imageBarrierBefore(kit.command_list.get(), current_image_index_);

    kit.command_list->RSSetViewports(1, constAddressOf(D3D12_VIEWPORT{
                                            .TopLeftX = 0.f,
                                            .TopLeftY = 0.f,
                                            .Width = FLOAT(image_extent_.width),
                                            .Height = FLOAT(image_extent_.height),
                                            .MinDepth = 0.f,
                                            .MaxDepth = 1.f,
                                        }));

    kit.command_list->RSSetScissorRects(1, constAddressOf(D3D12_RECT{
                                               .left = 0,
                                               .top = 0,
                                               .right = LONG(image_extent_.width),
                                               .bottom = LONG(image_extent_.height),
                                           }));

    const auto rtv_handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart() +
                            current_image_index_ * device_->getRtvDescriptorSize();

    const std::array<FLOAT, 4> color_rgba{clear_color.r, clear_color.g, clear_color.b, clear_color.a};
    kit.command_list->ClearRenderTargetView(rtv_handle, color_rgba.data(), 0, nullptr);

    if (use_depth_) {
        const auto dsv_handle = dsv_heap_->GetCPUDescriptorHandleForHeapStart() +
                                n_frame_ * device_->getDsvDescriptorSize();

        kit.command_list->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0,
                                                0, nullptr);

        kit.command_list->OMSetRenderTargets(1, &rtv_handle, true, &dsv_handle);
    } else {
        kit.command_list->OMSetRenderTargets(1, &rtv_handle, true, nullptr);
    }

    current_pipeline_->getLayout().bindRootSignature(kit.command_list.get());

    return render_target_status_;
}

bool RenderTarget::endRenderTarget() {
    auto& kit = frame_render_kits_[n_frame_];

    frame_image_provider_->imageBarrierAfter(kit.command_list.get(), current_image_index_);

    if (!DevQueue::closeCommandList(kit.command_list.get())) { return false; }

    device_->getDirectQueue().executeCommandLists(std::array{static_cast<ID3D12CommandList*>(kit.command_list.get())});

    render_target_status_ = frame_image_provider_->presentImage();

    kit.fence.queueSignal(device_->getDirectQueue());

    if (++current_image_index_ == image_count_) { current_image_index_ = 0; }
    if (++n_frame_ == frame_render_kits_.size()) { n_frame_ = 0; }
    return render_target_status_ <= RenderTargetResult::OUT_OF_DATE;
}

void RenderTarget::setViewport(const Rect& rect, float z_near, float z_far) {
    auto& kit = frame_render_kits_[n_frame_];
    kit.command_list->RSSetViewports(1, constAddressOf(D3D12_VIEWPORT{
                                            .TopLeftX = FLOAT(rect.offset.x),
                                            .TopLeftY = FLOAT(rect.offset.y),
                                            .Width = FLOAT(rect.extent.width),
                                            .Height = FLOAT(rect.extent.height),
                                            .MinDepth = FLOAT(z_near),
                                            .MaxDepth = FLOAT(z_far),
                                        }));
}

void RenderTarget::setScissor(const Rect& rect) {
    auto& kit = frame_render_kits_[n_frame_];
    kit.command_list->RSSetScissorRects(1, constAddressOf(D3D12_RECT{
                                               .left = LONG(rect.offset.x),
                                               .top = LONG(rect.offset.y),
                                               .right = LONG(rect.offset.x + rect.extent.width),
                                               .bottom = LONG(rect.offset.y + rect.extent.height),
                                           }));
}

void RenderTarget::bindPipeline(IPipeline& pipeline) {
    auto& kit = frame_render_kits_[n_frame_];
    auto* old_pipeline_layout = &current_pipeline_->getLayout();
    current_pipeline_ = &static_cast<Pipeline&>(pipeline);
    kit.command_list->SetPipelineState(current_pipeline_->getD3D12PipelineState());
    if (old_pipeline_layout != &current_pipeline_->getLayout()) {
        current_pipeline_->getLayout().bindRootSignature(kit.command_list.get());
    }
}

void RenderTarget::bindVertexBuffer(IBuffer& buffer, std::uint32_t slot, std::uint32_t stride, std::uint32_t offset) {
    auto& kit = frame_render_kits_[n_frame_];
    const auto& vbv = static_cast<Buffer&>(buffer).getVertexBufferView(
        offset, stride != 0 ? stride : current_pipeline_->getVertexStride(slot));
    kit.command_list->IASetVertexBuffers(slot, 1, &vbv);
}

void RenderTarget::bindDescriptorSet(IDescriptorSet& descriptor_set, std::uint32_t set_index) {
    auto& kit = frame_render_kits_[n_frame_];
    current_pipeline_->getLayout().bindRootDescriptorTables(kit.command_list.get(), set_index,
                                                            static_cast<DescriptorSet&>(descriptor_set), {});
}

void RenderTarget::bindDescriptorSetDynamic(IDescriptorSet& descriptor_set, std::uint32_t set_index,
                                            std::span<const std::uint32_t> offsets) {
    auto& kit = frame_render_kits_[n_frame_];
    current_pipeline_->getLayout().bindRootDescriptorTables(kit.command_list.get(), set_index,
                                                            static_cast<DescriptorSet&>(descriptor_set), offsets);
}

void RenderTarget::setPrimitiveTopology(PrimitiveTopology topology) {
    auto& kit = frame_render_kits_[n_frame_];
    kit.command_list->IASetPrimitiveTopology(TBL_D3D12_PRIMITIVE_TOPOLOGY[unsigned(topology)]);
}

void RenderTarget::drawGeometry(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex,
                                std::uint32_t first_instance) {
    auto& kit = frame_render_kits_[n_frame_];
    kit.command_list->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
}

//@}
