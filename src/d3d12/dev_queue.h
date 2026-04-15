#pragma once

#include "d3d12_api.h"

#include <span>

namespace app3d::rel::d3d12 {

class Device;

class DevQueue {
 public:
    DevQueue() = default;
    DevQueue(const DevQueue&) = delete;
    DevQueue& operator=(const DevQueue&) = delete;

    bool create(Device& device, D3D12_COMMAND_LIST_TYPE type);
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> createGraphicsCommandList();

    static bool closeCommandList(ID3D12GraphicsCommandList* command_list);

    bool resetCommandList(ID3D12GraphicsCommandList* command_list);

    void executeCommandLists(std::span<ID3D12CommandList* const> command_lists) {
        queue_->ExecuteCommandLists(UINT(command_lists.size()), command_lists.data());
    }

    ID3D12CommandQueue* operator~() { return queue_.Get(); }

 private:
    Device* device_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_;
};

}  // namespace app3d::rel::d3d12
