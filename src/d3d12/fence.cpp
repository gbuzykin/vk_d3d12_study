#include "fence.h"

#include "d3d12_logger.h"
#include "device.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// Fence class implementation

bool Fence::create(Device& device) {
    HRESULT result = device.getD3D12Device()->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                                          IID_PPV_ARGS(fence_.reset_and_get_address()));
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create fence: {}", D3D12Result(result));
        return false;
    }

    event_handle_ = ::CreateEventExW(nullptr, L"", 0, EVENT_ALL_ACCESS);
    if (event_handle_ == INVALID_HANDLE_VALUE) {
        logError(LOG_D3D12 "couldn't create Win32 event: {}", D3D12Result(result));
        return false;
    }

    current_fence_ = 0;
    return true;
}

bool Fence::queueSignal(DevQueue& queue) {
    ++current_fence_;

    HRESULT result = queue.getD3D12Queue()->Signal(fence_.get(), current_fence_);
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't queue fence signal: {}", D3D12Result(result));
        return false;
    }

    return true;
}

bool Fence::wait() {
    if (fence_->GetCompletedValue() >= current_fence_) { return true; }

    HRESULT result = fence_->SetEventOnCompletion(current_fence_, event_handle_);
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't set fence completion event: {}", D3D12Result(result));
        return false;
    }

    ::WaitForSingleObject(event_handle_, INFINITE);
    return true;
}
