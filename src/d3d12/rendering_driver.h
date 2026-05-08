#pragma once

#include "d3d12_api.h"

#include "interfaces/i_rendering_driver.h"
#include "rel/hlsl_compiler.h"

#include <string>
#include <vector>

namespace app3d::rel::d3d12 {

class PhysicalDevice {
 public:
    explicit PhysicalDevice(util::ref_ptr<IDXGIAdapter> adapter);
    PhysicalDevice(const PhysicalDevice&) = delete;
    PhysicalDevice& operator=(const PhysicalDevice&) = delete;

    const std::string& getName() const { return name_; }

    bool loadDescription();

    IDXGIAdapter* getAdapter() { return adapter_.get(); }

 private:
    util::ref_ptr<IDXGIAdapter> adapter_;
    DXGI_ADAPTER_DESC adapter_desc_{};
    std::string name_;
};

class RenderingDriver final : public util::ref_counter, public IRenderingDriver {
 public:
    RenderingDriver();
    ~RenderingDriver() override;

    IDXGIFactory4* getDXGIFactory() { return dxgi_factory_.get(); }

    //@{ IRenderingDriver
    util::ref_counter& getRefCounter() override { return *this; }
    bool init(const uxs::db::value& app_info) override;
    std::uint32_t getPhysicalDeviceCount() const override;
    const char* getPhysicalDeviceName(std::uint32_t device_index) const override;
    bool isSuitablePhysicalDevice(std::uint32_t device_index, const uxs::db::value& caps) const override;
    util::ref_ptr<ISurface> createSurface(const WindowDescriptor& win_desc) override;
    util::ref_ptr<IDevice> createDevice(std::uint32_t device_index, const uxs::db::value& caps) override;
    DataBlob compileShader(const DataBlob& source_text, const uxs::db::value& args, DataBlob& compiler_output) override;
    //@}

 private:
    util::ref_ptr<IDXGIFactory4> dxgi_factory_;
    std::vector<std::unique_ptr<PhysicalDevice>> physical_devices_;
    HlslCompiler hlsl_compiler_;

    bool loadPhysicalDeviceList();
};

}  // namespace app3d::rel::d3d12
