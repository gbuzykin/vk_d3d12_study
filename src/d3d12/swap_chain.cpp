#include "swap_chain.h"

#include "d3d12_logger.h"
#include "device.h"
#include "render_target.h"
#include "rendering_driver.h"
#include "surface.h"
#include "tables.h"
#include "wrappers.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// SwapChain class implementation

SwapChain::SwapChain(Device& device, Surface& surface)
    : device_(util::not_null{&device}), surface_(util::not_null{&surface}) {}

SwapChain::~SwapChain() {}

bool SwapChain::create(const uxs::db::value& opts) { return recreateSwapChainResources(opts); }

void SwapChain::imageBarrierBefore(ID3D12GraphicsCommandList* command_list, std::uint32_t image_index) {
    command_list->ResourceBarrier(1, constAddressOf(Wrapper<D3D12_RESOURCE_BARRIER>::transition({
                                         .resource = images_[image_index].get(),
                                         .state_before = D3D12_RESOURCE_STATE_PRESENT,
                                         .state_after = D3D12_RESOURCE_STATE_RENDER_TARGET,
                                     })));
}

void SwapChain::imageBarrierAfter(ID3D12GraphicsCommandList* command_list, std::uint32_t image_index) {
    command_list->ResourceBarrier(1, constAddressOf(Wrapper<D3D12_RESOURCE_BARRIER>::transition({
                                         .resource = images_[image_index].get(),
                                         .state_before = D3D12_RESOURCE_STATE_RENDER_TARGET,
                                         .state_after = D3D12_RESOURCE_STATE_PRESENT,
                                     })));
}

RenderTargetResult SwapChain::presentImage() {
    HRESULT result = swap_chain_->Present(0, 0);
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't present swap chain: {}", D3D12Result{result});
        return RenderTargetResult::FAILED;
    }
    return RenderTargetResult::SUCCESS;
}

void SwapChain::removeRenderTarget(RenderTarget* render_target) {
    if (render_target_ == render_target) { render_target_ = nullptr; }
}

//@{ ISwapChain

bool SwapChain::recreate(const uxs::db::value& opts) { return recreateSwapChainResources(opts); }

util::ref_ptr<IRenderTarget> SwapChain::createRenderTarget(const uxs::db::value& opts) {
    auto render_target = util::make_new<RenderTarget>(*device_, *this);
    if (!render_target->create(opts)) { return nullptr; }
    render_target_ = render_target.get();
    return std::move(render_target);
}

//@}

bool SwapChain::recreateSwapChainResources(const uxs::db::value& opts) {
    struct WindowDescImpl {
        HINSTANCE hinstance;
        HWND hwnd;
    };

    const auto& win_desc = surface_->getWindowDescriptor();
    static_assert(sizeof(win_desc.handle) >= sizeof(WindowDescImpl), "Too little WindowDescriptor size");
    static_assert(std::alignment_of_v<decltype(win_desc.handle)> >= std::alignment_of_v<WindowDescImpl>,
                  "Too little WindowDescriptor alignment");
    const WindowDescImpl& win_desc_impl = *reinterpret_cast<const WindowDescImpl*>(&win_desc.handle);

    if (render_target_) { render_target_->destroyFrameResources(); }
    images_.clear();

    RECT client_rect{};
    ::GetClientRect(win_desc_impl.hwnd, &client_rect);

    image_extent_.width = std::uint32_t(client_rect.right - client_rect.left);
    image_extent_.height = std::uint32_t(client_rect.bottom - client_rect.top);

    const auto d3d12_back_buffer_format = TBL_D3D12_FORMAT[unsigned(BACK_BUFFER_FORMAT)];

    if (!swap_chain_) {
        DXGI_SWAP_CHAIN_DESC swap_chain_desc{
            .BufferDesc =
                {
                    .Width = UINT(image_extent_.width),
                    .Height = UINT(image_extent_.height),
                    .RefreshRate = {.Numerator = 60, .Denominator = 1},
                    .Format = d3d12_back_buffer_format,
                    .ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
                    .Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
                },
            .SampleDesc = {.Count = 1, .Quality = 0},
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = UINT(SWAP_CHAIN_BUFFER_COUNT),
            .OutputWindow = win_desc_impl.hwnd,
            .Windowed = TRUE,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH,
        };

        HRESULT result = surface_->getInstance().getDXGIFactory()->CreateSwapChain(
            device_->getDirectQueue().getD3D12Queue(), &swap_chain_desc, swap_chain_.reset_and_get_address());
        if (result != S_OK) {
            logError(LOG_D3D12 "couldn't create swap chain: {}", D3D12Result(result));
            return false;
        }
    } else {
        HRESULT result = swap_chain_->ResizeBuffers(SWAP_CHAIN_BUFFER_COUNT, image_extent_.width, image_extent_.height,
                                                    d3d12_back_buffer_format, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
        if (result != S_OK) {
            logError(LOG_D3D12 "couldn't resize swap chain: {}", D3D12Result(result));
            return false;
        }
    }

    images_.resize(SWAP_CHAIN_BUFFER_COUNT);
    for (std::uint32_t n = 0; n < SWAP_CHAIN_BUFFER_COUNT; ++n) {
        HRESULT result = swap_chain_->GetBuffer(n, IID_PPV_ARGS(images_[n].reset_and_get_address()));
        if (result != S_OK) {
            logError(LOG_D3D12 "couldn't get swap chain image buffer: {}", D3D12Result(result));
            return false;
        }
    }

    if (render_target_ && !render_target_->createFrameResources()) { return false; }

    return true;
}
