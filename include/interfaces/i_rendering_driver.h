#pragma once

#include <cstdint>
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

struct ISurface;
struct ISwapChain;

struct Version {
    std::uint32_t major;
    std::uint32_t minor;
    std::uint32_t patch;
};

struct ApplicationInfo {
    const char* name;
    Version version;
};

struct DesiredDeviceCaps {
    bool needs_compute;
};

struct SwapChainCreateInfo {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t layer_count;
};

struct WindowHandle {
    alignas(std::alignment_of_v<void*>) std::uint8_t v[2 * sizeof(void*)];
};

struct IDevice {
    virtual ~IDevice() = default;
    virtual ISwapChain* createSwapChain(ISurface& surface, const SwapChainCreateInfo& create_info) = 0;
};

struct ISurface {
    virtual ~ISurface() = default;
};

struct ISwapChain {
    virtual ~ISwapChain() = default;
};

struct IRenderingDriver {
    virtual ~IRenderingDriver() = default;
    virtual bool init(const ApplicationInfo& app_info) = 0;
    virtual std::uint32_t getPhysicalDeviceCount() const = 0;
    virtual const char* getPhysicalDeviceName(std::uint32_t device_index) const = 0;
    virtual bool isSuitablePhysicalDevice(std::uint32_t device_index, const DesiredDeviceCaps& caps) const = 0;
    virtual ISurface* createSurface(const WindowHandle& window_handle) = 0;
    virtual IDevice* createDevice(std::uint32_t device_index, const DesiredDeviceCaps& caps) = 0;
};

// Registered rendering driver descriptor
struct DriverDesc {
    std::string_view name;
    std::unique_ptr<IRenderingDriver> (*create_func)();
};

using GetDriverDescriptorFuncPtr = const DriverDesc* (*)();

}  // namespace app3d::rel
