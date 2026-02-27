#pragma once

#include "window_descriptor.h"

#include <memory>
#include <string_view>

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

struct IDevice {
    virtual ~IDevice() = default;
};

struct ISurface {
    virtual ~ISurface() = default;
};

struct IRenderingDriver {
    virtual ~IRenderingDriver() = default;
    virtual bool init(const ApplicationInfo& app_info) = 0;
    virtual std::uint32_t getPhysicalDeviceCount() const = 0;
    virtual const char* getPhysicalDeviceName(std::uint32_t device_index) const = 0;
    virtual bool isSuitablePhysicalDevice(std::uint32_t device_index, const DesiredDeviceCaps& caps) const = 0;
    virtual ISurface* createSurface(const WindowDescriptor& window_desc) = 0;
    virtual IDevice* createDevice(std::uint32_t device_index, ISurface& surface, const DesiredDeviceCaps& caps) = 0;
};

// Registered rendering driver descriptor
struct DriverDesc {
    std::string_view name;
    std::unique_ptr<IRenderingDriver> (*create_func)();
};

using GetDriverDescriptorFuncPtr = const DriverDesc* (*)();

}  // namespace app3d::rel
