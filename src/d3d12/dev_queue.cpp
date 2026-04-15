#include "dev_queue.h"

#include "d3d12_logger.h"
#include "device.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// DevQueue class implementation

bool DevQueue::create(Device& device, D3D12_COMMAND_LIST_TYPE type) {
    device_ = &device;

    const D3D12_COMMAND_QUEUE_DESC queue_desc{
        .Type = type,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
    };

    HRESULT result = (~*device_)->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue_));
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create command queue: {}", result);
        return false;
    }

    result = (~*device_)->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator_));
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create command allocator: {}", result);
        return false;
    }

    return true;
}

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> DevQueue::createGraphicsCommandList() {
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list;
    HRESULT result = (~*device_)->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr,
                                                    IID_PPV_ARGS(&command_list));
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create graphics command list: {}", result);
        return nullptr;
    }

    // Start off in a closed state. This is because the first time we refer to the command list we will Reset it,
    // and it needs to be closed before calling Reset
    if (!closeCommandList(command_list.Get())) { return nullptr; }

    return command_list;
}

bool DevQueue::closeCommandList(ID3D12GraphicsCommandList* command_list) {
    HRESULT result = command_list->Close();
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't close command list: {}", result);
        return false;
    }
    return true;
}

bool DevQueue::resetCommandList(ID3D12GraphicsCommandList* command_list) {
    HRESULT result = command_list->Reset(allocator_.Get(), nullptr);
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't reset command list: {}", result);
        return false;
    }
    return true;
}
