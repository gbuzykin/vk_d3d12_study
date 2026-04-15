#include "rendering_driver.h"

#include "d3d12_logger.h"
#include "device.h"
#include "surface.h"

#include <uxs/string_util.h>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// RenderingDriver class implementation

RenderingDriver::RenderingDriver() {}

RenderingDriver::~RenderingDriver() { logDebug(LOG_D3D12 "destroy RenderingDriver"); }

//@{ IRenderingDriver

bool RenderingDriver::init(const uxs::db::value& app_info) {
#if !defined(NDEBUG)
    {  // Enable the D3D12 debug layer.
        Microsoft::WRL::ComPtr<ID3D12Debug> debug_controller;
        HRESULT result = D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller));
        if (result != S_OK) {
            logError(LOG_D3D12 "couldn't get debug interface: {}", D3D12Result{result});
            return false;
        }
        debug_controller->EnableDebugLayer();
    }
#endif

    HRESULT result = CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory_));
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create DXGI factory: {}", D3D12Result{result});
        return false;
    }

    return loadPhysicalDeviceList();
}

std::uint32_t RenderingDriver::getPhysicalDeviceCount() const { return std::uint32_t(physical_devices_.size()); }

const char* RenderingDriver::getPhysicalDeviceName(std::uint32_t device_index) const {
    return physical_devices_[device_index]->getName().c_str();
}

bool RenderingDriver::isSuitablePhysicalDevice(std::uint32_t device_index, const uxs::db::value& caps) const {
    return true;
}

util::ref_ptr<ISurface> RenderingDriver::createSurface(const WindowDescriptor& win_desc) {
    auto surface = util::make_new<Surface>(*this);
    if (!surface->create(win_desc)) { return nullptr; }
    return std::move(surface);
}

util::ref_ptr<IDevice> RenderingDriver::createDevice(std::uint32_t device_index, const uxs::db::value& caps) {
    auto& physical_device = *physical_devices_[device_index];
    auto device = util::make_new<Device>(*this, physical_device);
    if (!device->create(caps)) { return nullptr; }
    return std::move(device);
}

//@}

bool RenderingDriver::loadPhysicalDeviceList() {
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    for (UINT i = 0; dxgi_factory_->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        auto physical_device = std::make_unique<PhysicalDevice>(std::move(adapter));
        if (physical_device->loadDescription()) { physical_devices_.emplace_back(std::move(physical_device)); }
    }
    return true;
}

// --------------------------------------------------------
// PhysicalDevice class implementation

PhysicalDevice::PhysicalDevice(Microsoft::WRL::ComPtr<IDXGIAdapter> adapter) : adapter_(std::move(adapter)) {}

bool PhysicalDevice::loadDescription() {
    adapter_->GetDesc(&adapter_desc_);
    name_ = uxs::utf8_string_adapter{}(adapter_desc_.Description);
    return true;
}

APP3D_REGISTER_RENDERING_DRIVER("D3D12", RenderingDriver);
