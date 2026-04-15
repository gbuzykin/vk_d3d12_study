#include "render_target.h"

#include "d3d12_logger.h"
#include "descriptor_set.h"
#include "device.h"
#include "pipeline.h"
#include "pipeline_layout.h"
#include "swap_chain.h"

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
    for (auto& kit : frame_render_kits_) {
        if (!kit.fence.create(*device_)) { return false; }
        kit.command_list = device_->getDirectQueue().createGraphicsCommandList();
    }

    const D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = image_count_,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0,
    };

    HRESULT result = (~*device_)->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap_));
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create RTV heap: {}", result);
        return false;
    }

    if (use_depth_) {
        const D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc{
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            .NumDescriptors = fif_count,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0,
        };

        result = (~*device_)->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&dsv_heap_));
        if (result != S_OK) {
            logError(LOG_D3D12 "couldn't create DSV heap: {}", result);
            return false;
        }
    }

    return true;
}

bool RenderTarget::createFrameResources() {
    image_extent_ = frame_image_provider_->getImageExtent();

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < image_count_; ++i) {
        (~*device_)->CreateRenderTargetView(frame_image_provider_->getImageResource(i), nullptr, rtv_handle);
        rtv_handle.ptr += device_->getRtvDescriptorSize();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = dsv_heap_->GetCPUDescriptorHandleForHeapStart();

    for (auto& kit : frame_render_kits_) {
        const D3D12_RESOURCE_DESC depth_stencil_desc{
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Alignment = 0,
            .Width = UINT(image_extent_.width),
            .Height = UINT(image_extent_.height),
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_R24G8_TYPELESS,
            .SampleDesc = {.Count = 1, .Quality = 0},
            .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
            .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
        };

        const D3D12MA::ALLOCATION_DESC allocation_desc = {
            .HeapType = D3D12_HEAP_TYPE_DEFAULT,
        };

        const D3D12_CLEAR_VALUE clear_value{
            .Format = depth_stencil_format_,
            .DepthStencil = {.Depth = 1.0f, .Stencil = 0},
        };

        HRESULT result = device_->getAllocator()->CreateResource(
            &allocation_desc, &depth_stencil_desc, D3D12_RESOURCE_STATE_COMMON, &clear_value,
            &kit.depth_stencil_allocation, IID_PPV_ARGS(&kit.depth_stencil_resource));
        if (result != S_OK) {
            logError(LOG_D3D12 "couldn't create depth&stencil resource: {}", result);
            return false;
        }

        const D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{
            .Format = depth_stencil_format_,
            .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
            .Flags = D3D12_DSV_FLAG_NONE,
        };

        (~*device_)->CreateDepthStencilView(kit.depth_stencil_resource.Get(), &dsv_desc, dsv_handle);
        dsv_handle.ptr += device_->getDsvDescriptorSize();

        if (!device_->getDirectQueue().resetCommandList(kit.command_list.Get())) { return false; }

        const D3D12_RESOURCE_BARRIER resource_barrier{
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition =
                {
                    .pResource = kit.depth_stencil_resource.Get(),
                    .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    .StateBefore = D3D12_RESOURCE_STATE_COMMON,
                    .StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE,
                },
        };

        // Transition the resource from its initial state to be used as a depth buffer
        kit.command_list->ResourceBarrier(1, &resource_barrier);

        if (!DevQueue::closeCommandList(kit.command_list.Get())) { return false; }

        device_->getDirectQueue().executeCommandLists(
            std::array{static_cast<ID3D12CommandList*>(kit.command_list.Get())});
    }

    render_target_status_ = RenderTargetResult::SUCCESS;
    return true;
}

void RenderTarget::destroyFrameResources() {
    for (auto& kit : frame_render_kits_) {
        kit.fence.queueSignal(device_->getDirectQueue());
        kit.fence.wait();
        kit.depth_stencil_resource.Reset();
        kit.depth_stencil_allocation.Reset();
    }
}

//@{ IRenderTarget

RenderTargetResult RenderTarget::beginRenderTarget(const Color4f& clear_color, float depth, std::uint32_t stencil) {
    if (render_target_status_ > RenderTargetResult::SUBOPTIMAL) { return render_target_status_; }

    if (++n_frame_ == frame_render_kits_.size()) { n_frame_ = 0; }
    auto& kit = frame_render_kits_[n_frame_];

    if (!kit.fence.queueSignal(device_->getDirectQueue())) { return RenderTargetResult::FAILED; }
    if (!kit.fence.wait()) { return RenderTargetResult::FAILED; }

    current_pipeline_ = nullptr;
    current_image_index_ = 0;
    return render_target_status_;
}

bool RenderTarget::endRenderTarget() { return render_target_status_ <= RenderTargetResult::OUT_OF_DATE; }

void RenderTarget::setViewport(const Rect& rect, float z_near, float z_far) {}

void RenderTarget::setScissor(const Rect& rect) {}

void RenderTarget::bindPipeline(IPipeline& pipeline) { current_pipeline_ = &static_cast<Pipeline&>(pipeline); }

void RenderTarget::bindVertexBuffer(IBuffer& buffer, std::size_t offset, std::uint32_t slot) {}

void RenderTarget::bindDescriptorSet(IDescriptorSet& descriptor_set, std::uint32_t set_index) {}

void RenderTarget::setPrimitiveTopology(PrimitiveTopology topology) {}

void RenderTarget::drawGeometry(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex,
                                std::uint32_t first_instance) {}

//@}
