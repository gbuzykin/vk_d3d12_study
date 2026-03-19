#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <string_view>
#include <type_traits>

#if defined(WIN32)
#    define APP3D_ENTRY_EXPORT extern "C" __declspec(dllexport)
#elif defined(__linux__)
#    define APP3D_ENTRY_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#define APP3D_REGISTER_RENDERING_DRIVER(name, driver_class) \
    constexpr app3d::rel::DriverDesc g_rendering_driver_descriptor{ \
        name, []() -> std::unique_ptr<app3d::rel::IRenderingDriver> { return std::make_unique<driver_class>(); }}; \
    APP3D_ENTRY_EXPORT const app3d::rel::DriverDesc* app3dGetRenderingDriverDescriptor() { \
        return &g_rendering_driver_descriptor; \
    } \
    static_assert(true)

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

struct IRenderingDriver {
    virtual ~IRenderingDriver() = default;
};

// Registered rendering driver descriptor
struct DriverDesc {
    std::string_view name;
    std::unique_ptr<IRenderingDriver> (*create_func)();
};

using GetDriverDescriptorFuncPtr = const DriverDesc* (*)();

}  // namespace app3d::rel
