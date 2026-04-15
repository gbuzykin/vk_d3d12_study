#pragma once

#include "d3d12_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::d3d12 {

class Device;

class Buffer : public util::ref_counter, public IBuffer {
 public:
    explicit Buffer(Device& device);
    ~Buffer() override;

    bool create();

    //@{ IBuffer
    util::ref_counter& getRefCounter() override { return *this; }
    bool updateVertexBuffer(std::span<const std::uint8_t> data, std::size_t offset) override;
    bool updateConstantBuffer(std::span<const std::uint8_t> data, std::size_t offset) override;
    //@}

 private:
    util::ref_ptr<Device> device_;
};

}  // namespace app3d::rel::d3d12
