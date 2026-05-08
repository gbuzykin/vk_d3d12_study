#pragma once

#include "d3d12_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::d3d12 {

class Device;

class Buffer : public util::ref_counter, public IBuffer {
 public:
    explicit Buffer(Device& device);
    ~Buffer() override;

    bool create(BufferType type, UINT64 size);

    ID3D12Resource* getResource() { return allocation_->GetResource(); }
    D3D12_VERTEX_BUFFER_VIEW getVertexBufferView(std::uint32_t offset, std::uint32_t element_stride);
    D3D12_CONSTANT_BUFFER_VIEW_DESC getConstantBufferViewDesc(std::uint64_t offset);

    //@{ IBuffer
    util::ref_counter& getRefCounter() override { return *this; }
    bool updateBuffer(std::span<const std::uint8_t> data, std::uint64_t offset) override;
    //@}

 private:
    util::ref_ptr<Device> device_;
    BufferType type_{};
    UINT64 size_ = 0;
    UINT64 alignment_ = 1;
    util::ref_ptr<D3D12MA::Allocation> allocation_;
};

}  // namespace app3d::rel::d3d12
