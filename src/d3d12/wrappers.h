#pragma once

#include "d3d12_api.h"

namespace app3d::rel::d3d12 {

template<typename Ty>
struct Wrapper;

template<>
struct Wrapper<D3D12_RESOURCE_BARRIER> {
    struct Transition {
        ID3D12Resource* resource;
        D3D12_RESOURCE_STATES state_before;
        D3D12_RESOURCE_STATES state_after;
        std::uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    };
    static D3D12_RESOURCE_BARRIER transition(Transition wrapper) {
        return {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = wrapper.flags,
            .Transition =
                {
                    .pResource = wrapper.resource,
                    .Subresource = UINT(wrapper.subresource),
                    .StateBefore = wrapper.state_before,
                    .StateAfter = wrapper.state_after,
                },
        };
    }
};

}  // namespace app3d::rel::d3d12
