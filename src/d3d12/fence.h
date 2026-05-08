#pragma once

#include "d3d12_api.h"

#include "util/ref_ptr.h"

namespace app3d::rel::d3d12 {

class Device;
class DevQueue;

class Fence {
 public:
    Fence() = default;

    ~Fence() {
        if (event_handle_ != INVALID_HANDLE_VALUE) { ::CloseHandle(event_handle_); }
    }

    Fence(Fence&& other) noexcept
        : fence_(std::move(other.fence_)), event_handle_(other.event_handle_), current_fence_(other.current_fence_) {
        other.event_handle_ = INVALID_HANDLE_VALUE;
    }

    Fence& operator=(Fence&& other) noexcept {
        if (&other == this) { return *this; }
        fence_ = std::move(other.fence_);
        event_handle_ = other.event_handle_;
        current_fence_ = other.current_fence_;
        other.event_handle_ = INVALID_HANDLE_VALUE;
        return *this;
    }

    bool create(Device& device);
    bool queueSignal(DevQueue& queue);
    bool wait();

 private:
    util::ref_ptr<ID3D12Fence> fence_;
    HANDLE event_handle_ = INVALID_HANDLE_VALUE;
    UINT64 current_fence_ = 0;
};

}  // namespace app3d::rel::d3d12
