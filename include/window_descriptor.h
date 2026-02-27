#pragma once

#include <cstdint>
#include <type_traits>

namespace app3d {

struct WindowDescriptor {
    alignas(std::alignment_of_v<void*>) std::uint8_t v[2 * sizeof(void*)];
};

}  // namespace app3d
