#pragma once

#include "fence.h"

#include <uxs/dynarray.h>

#include <span>

namespace app3d::rel::d3d12 {

class Device;

class DevQueue {
 public:
    DevQueue() = default;
    DevQueue(const DevQueue&) = delete;
    DevQueue& operator=(const DevQueue&) = delete;

    bool create(Device& device, D3D12_COMMAND_LIST_TYPE type, std::uint32_t allocator_count);
    bool growAllocatorCount(std::uint32_t allocator_count);
    bool resetAllocator(std::uint32_t allocator_index);
    bool wait();

    util::ref_ptr<ID3D12GraphicsCommandList> createGraphicsCommandList(std::uint32_t allocator_index);
    static bool closeCommandList(ID3D12GraphicsCommandList* command_list);
    bool resetCommandList(std::uint32_t allocator_index, ID3D12GraphicsCommandList* command_list,
                          ID3D12PipelineState* pipeline);

    void executeCommandLists(std::span<ID3D12CommandList* const> command_lists) {
        queue_->ExecuteCommandLists(UINT(command_lists.size()), command_lists.data());
    }

    ID3D12CommandQueue* getD3D12Queue() { return queue_.get(); }

 private:
    Device* device_ = nullptr;
    Fence fence_;
    D3D12_COMMAND_LIST_TYPE type_{};
    util::ref_ptr<ID3D12CommandQueue> queue_;
    uxs::inline_dynarray<util::ref_ptr<ID3D12CommandAllocator>, 8> allocators_;
};

}  // namespace app3d::rel::d3d12
