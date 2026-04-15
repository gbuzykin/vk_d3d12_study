#pragma once

#include "d3d12_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::d3d12 {

class Device;

class PipelineLayout : public util::ref_counter, public IPipelineLayout {
 public:
    explicit PipelineLayout(Device& device);
    ~PipelineLayout();

    bool create(const uxs::db::value& config);

    //@{ IPipelineLayout
    util::ref_counter& getRefCounter() override { return *this; }
    //@}

 private:
    util::ref_ptr<Device> device_;
};

}  // namespace app3d::rel::d3d12
