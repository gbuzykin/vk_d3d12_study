#pragma once

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::d3d12 {

class RenderingDriver;

class Surface final : public util::ref_counter, public ISurface {
 public:
    explicit Surface(RenderingDriver& instance);
    ~Surface() override;

    const WindowDescriptor& getWindowDescriptor() const { return win_desc_; }

    bool create(const WindowDescriptor& win_desc);

    RenderingDriver& getInstance() { return *instance_; }

    //@{ ISurface
    util::ref_counter& getRefCounter() override { return *this; }
    util::ref_ptr<ISwapChain> createSwapChain(IDevice& device, const uxs::db::value& opts) override;
    //@}

 private:
    util::ref_ptr<RenderingDriver> instance_;
    WindowDescriptor win_desc_;
};

}  // namespace app3d::rel::d3d12
