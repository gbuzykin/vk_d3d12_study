#pragma once

#include <cstdint>
#include <limits>

namespace app3d {

constexpr std::uint32_t INVALID_UINT32_VALUE = std::numeric_limits<std::uint32_t>::max();

template<typename Ty>
const Ty* constAddressOf(Ty&& o) {
    return &static_cast<const Ty&>(o);
}

}  // namespace app3d
