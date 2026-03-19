#pragma once

#include <cstdint>
#include <type_traits>

namespace app3d::rel {

enum class PlatformType {
    PLATFORM_WIN32 = 0,
    PLATFORM_XLIB,
    PLATFORM_XCB,
    PLATFORM_WAYLAND,
};

struct WindowDescriptor {
    PlatformType platform;
    struct HandlePlaceholder {
        alignas(std::alignment_of_v<void*>) std::uint8_t handle[2 * sizeof(void*)];
    } handle;
};

}  // namespace app3d::rel
