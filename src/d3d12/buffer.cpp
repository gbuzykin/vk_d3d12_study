#include "buffer.h"

#include "d3d12_logger.h"
#include "device.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// Buffer class implementation

Buffer::Buffer(Device& device) : device_(util::not_null{&device}) {}

Buffer::~Buffer() {}

bool Buffer::create(BufferType type, UINT64 size) {
    if (type == BufferType::CONSTANT) {
        alignment_ = 256;
        size = (size + alignment_ - 1) & ~(alignment_ - 1);
    }

    const D3D12MA::ALLOCATION_DESC allocation_desc = {
        .HeapType = D3D12_HEAP_TYPE_DEFAULT,
    };

    const D3D12_RESOURCE_DESC resource_desc{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = size,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE,
    };

    HRESULT result = device_->getAllocator()->CreateResource(&allocation_desc, &resource_desc,
                                                             D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                             allocation_.reset_and_get_address(), IID_NULL, nullptr);
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create buffer resource: {}", D3D12Result(result));
        return false;
    }

    type_ = type;
    size_ = size;
    return true;
}

D3D12_VERTEX_BUFFER_VIEW Buffer::getVertexBufferView(std::uint32_t offset, std::uint32_t element_stride) {
    return {
        .BufferLocation = D3D12_GPU_VIRTUAL_ADDRESS(allocation_->GetResource()->GetGPUVirtualAddress() + offset),
        .SizeInBytes = UINT(size_ - offset),
        .StrideInBytes = UINT(element_stride),
    };
}

D3D12_CONSTANT_BUFFER_VIEW_DESC Buffer::getConstantBufferViewDesc(std::uint64_t offset) {
    return {
        .BufferLocation = allocation_->GetResource()->GetGPUVirtualAddress() + offset,
        .SizeInBytes = UINT(size_ - offset),
    };
}

//@{ IBuffer

bool Buffer::updateBuffer(std::span<const std::uint8_t> data, std::uint64_t offset) {
    offset = (offset + alignment_ - 1) & ~(alignment_ - 1);
    return device_->updateBuffer(data, allocation_->GetResource(), offset, D3D12_RESOURCE_STATE_COMMON,
                                 D3D12_RESOURCE_STATE_GENERIC_READ);
}

//@}
