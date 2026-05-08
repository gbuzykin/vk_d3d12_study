#pragma once

#include <D3D12MemAlloc.h>
#include <d3d12.h>

#include <cstdint>

#ifdef min
#    undef min
#endif

#ifdef max
#    undef max
#endif

namespace app3d::rel::d3d12 {

inline D3D12_CPU_DESCRIPTOR_HANDLE& operator+=(D3D12_CPU_DESCRIPTOR_HANDLE& handle, std::uint32_t offset) {
    handle.ptr += offset;
    return handle;
}

inline D3D12_CPU_DESCRIPTOR_HANDLE operator+(D3D12_CPU_DESCRIPTOR_HANDLE handle, std::uint32_t offset) {
    handle.ptr += offset;
    return handle;
}

inline D3D12_GPU_DESCRIPTOR_HANDLE& operator+=(D3D12_GPU_DESCRIPTOR_HANDLE& handle, std::uint32_t offset) {
    handle.ptr += offset;
    return handle;
}

inline D3D12_GPU_DESCRIPTOR_HANDLE operator+(D3D12_GPU_DESCRIPTOR_HANDLE handle, std::uint32_t offset) {
    handle.ptr += offset;
    return handle;
}

template<typename Ty>
const Ty* constAddressOf(Ty&& o) {
    return &static_cast<const Ty&>(o);
}

}  // namespace app3d::rel::d3d12
