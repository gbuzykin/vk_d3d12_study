#include "dev_queue.h"

#include "d3d12_logger.h"
#include "device.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// DevQueue class implementation

bool DevQueue::create(Device& device, D3D12_COMMAND_LIST_TYPE type, std::uint32_t allocator_count) {
    device_ = &device;

    HRESULT result = device_->getD3D12Device()->CreateCommandQueue(
        constAddressOf(D3D12_COMMAND_QUEUE_DESC{.Type = type, .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE}),
        IID_PPV_ARGS(queue_.reset_and_get_address()));
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create command queue: {}", D3D12Result(result));
        return false;
    }

    if (!fence_.create(*device_)) { return false; }

    type_ = type;
    return allocator_count ? growAllocatorCount(allocator_count) : true;
}

bool DevQueue::growAllocatorCount(std::uint32_t allocator_count) {
    const std::uint32_t old_allocator_count = std::uint32_t(allocators_.size());
    if (allocator_count <= old_allocator_count) { return true; }

    allocators_.resize(allocator_count);

    for (std::uint32_t n = old_allocator_count; n < allocator_count; ++n) {
        HRESULT result = device_->getD3D12Device()->CreateCommandAllocator(
            type_, IID_PPV_ARGS(allocators_[n].reset_and_get_address()));
        if (result != S_OK) {
            logError(LOG_D3D12 "couldn't create command allocator: {}", D3D12Result(result));
            return false;
        }
    }

    return true;
}

bool DevQueue::resetAllocator(std::uint32_t allocator_index) {
    HRESULT result = allocators_[allocator_index]->Reset();
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't reset command allocator: {}", D3D12Result(result));
        return false;
    }
    return true;
}

bool DevQueue::wait() {
    if (!fence_.queueSignal(*this)) { return false; }
    if (!fence_.wait()) { return false; }
    return true;
}

util::ref_ptr<ID3D12GraphicsCommandList> DevQueue::createGraphicsCommandList(std::uint32_t allocator_index) {
    util::ref_ptr<ID3D12GraphicsCommandList> command_list;
    HRESULT result = device_->getD3D12Device()->CreateCommandList(0, type_, allocators_[allocator_index].get(), nullptr,
                                                                  IID_PPV_ARGS(command_list.reset_and_get_address()));
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create graphics command list: {}", D3D12Result(result));
        return nullptr;
    }

    // Start off in a closed state. This is because the first time we refer to the command list we will Reset it,
    // and it needs to be closed before calling Reset
    if (!closeCommandList(command_list.get())) { return nullptr; }

    return command_list;
}

bool DevQueue::closeCommandList(ID3D12GraphicsCommandList* command_list) {
    HRESULT result = command_list->Close();
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't close command list: {}", D3D12Result(result));
        return false;
    }
    return true;
}

bool DevQueue::resetCommandList(std::uint32_t allocator_index, ID3D12GraphicsCommandList* command_list,
                                ID3D12PipelineState* pipeline) {
    HRESULT result = command_list->Reset(allocators_[allocator_index].get(), pipeline);
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't reset command list: {}", D3D12Result(result));
        return false;
    }
    return true;
}
