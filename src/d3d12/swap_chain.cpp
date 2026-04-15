#include "swap_chain.h"

#include "d3d12_logger.h"
#include "device.h"
#include "render_target.h"
#include "rendering_driver.h"
#include "surface.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// SwapChain class implementation

SwapChain::SwapChain(Device& device, Surface& surface)
    : device_(util::not_null{&device}), surface_(util::not_null{&surface}) {}

SwapChain::~SwapChain() {}

bool SwapChain::create(const uxs::db::value& opts) { return recreateSwapChainResources(opts); }

void SwapChain::removeRenderTarget(RenderTarget* render_target) {
    if (render_target_ == render_target) { render_target_ = nullptr; }
}

//@{ ISwapChain

bool SwapChain::recreateSwapChain(const uxs::db::value& opts) { return recreateSwapChainResources(opts); }

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

    if (!swap_chain_) {
        DXGI_SWAP_CHAIN_DESC swap_chain_desc{
            .BufferDesc =
                {
                    .Width = UINT(image_extent_.width),
                    .Height = UINT(image_extent_.height),
                    .RefreshRate = {.Numerator = 60, .Denominator = 1},
                    .Format = back_buffer_format,
                    .ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
                    .Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
                },
            .SampleDesc = {.Count = 1, .Quality = 0},
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = swap_chain_buffer_count,
            .OutputWindow = win_desc_impl.hwnd,
            .Windowed = true,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH,
        };

        HRESULT result = surface_->getInstance().getDXGIFactory()->CreateSwapChain(~device_->getDirectQueue(),
                                                                                   &swap_chain_desc, &swap_chain_);
        if (result != S_OK) {
            logError(LOG_D3D12 "couldn't create swap chain: {}", result);
            return false;
        }
    } else {
        HRESULT result = swap_chain_->ResizeBuffers(swap_chain_buffer_count, UINT(image_extent_.width),
                                                    UINT(image_extent_.height), back_buffer_format,
                                                    DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
        if (result != S_OK) {
            logError(LOG_D3D12 "couldn't resize swap chain: {}", result);
            return false;
        }
    }

    images_.resize(swap_chain_buffer_count);
    for (UINT i = 0; i < swap_chain_buffer_count; ++i) {
        HRESULT result = swap_chain_->GetBuffer(i, IID_PPV_ARGS(&images_[i]));
        if (result != S_OK) {
            logError(LOG_D3D12 "couldn't get swap chain image buffer: {}", result);
            return false;
        }
    }

    if (render_target_ && !render_target_->createFrameResources()) { return false; }

    return true;
}
