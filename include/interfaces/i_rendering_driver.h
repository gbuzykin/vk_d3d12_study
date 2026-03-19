#pragma once

#include <uxs/db/value.h>

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

constexpr std::uint32_t INVALID_UINT32_VALUE = std::numeric_limits<std::uint32_t>::max();

enum class PlatformType {
    PLATFORM_WIN32 = 0,
    PLATFORM_XLIB,
    PLATFORM_XCB,
    PLATFORM_WAYLAND,
};

struct ISurface;
struct ISwapChain;

struct IDevice {
    virtual ~IDevice() = default;
    virtual bool prepareTestScene(ISurface& surface) = 0;
    virtual bool renderTestScene(ISwapChain& swap_chain) = 0;
};

struct ISurface {
    virtual ~ISurface() = default;
    virtual ISwapChain* createSwapChain(IDevice& device, const uxs::db::value& opts) = 0;
};

struct ISwapChain {
    virtual ~ISwapChain() = default;
};

struct WindowDescriptor {
    PlatformType platform;
    struct HandlePlaceholder {
        alignas(std::alignment_of_v<void*>) std::uint8_t handle[2 * sizeof(void*)];
    } handle;
};

struct IRenderingDriver {
    virtual ~IRenderingDriver() = default;
    virtual bool init(const uxs::db::value& app_info) = 0;
    virtual std::uint32_t getPhysicalDeviceCount() const = 0;
    virtual const char* getPhysicalDeviceName(std::uint32_t device_index) const = 0;
    virtual bool isSuitablePhysicalDevice(std::uint32_t device_index, const uxs::db::value& caps) const = 0;
    virtual ISurface* createSurface(const WindowDescriptor& win_desc) = 0;
    virtual IDevice* createDevice(std::uint32_t device_index, const uxs::db::value& caps) = 0;
};

// Registered rendering driver descriptor
struct DriverDesc {
    std::string_view name;
    std::unique_ptr<IRenderingDriver> (*create_func)();
};

using GetDriverDescriptorFuncPtr = const DriverDesc* (*)();

}  // namespace app3d::rel
