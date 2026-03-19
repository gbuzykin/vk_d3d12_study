#pragma once

#include <uxs/db/value.h>

#include <cstdint>
#include <limits>
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

constexpr std::uint32_t INVALID_UINT32_VALUE = std::numeric_limits<std::uint32_t>::max();

struct IDevice {
    virtual ~IDevice() = default;
};

struct IRenderingDriver {
    virtual ~IRenderingDriver() = default;
    virtual bool init(const uxs::db::value& app_info) = 0;
    virtual std::uint32_t getPhysicalDeviceCount() const = 0;
    virtual const char* getPhysicalDeviceName(std::uint32_t device_index) const = 0;
    virtual bool isSuitablePhysicalDevice(std::uint32_t device_index, const uxs::db::value& caps) const = 0;
    virtual IDevice* createDevice(std::uint32_t device_index, const uxs::db::value& caps) = 0;
};

// Registered rendering driver descriptor
struct DriverDesc {
    std::string_view name;
    std::unique_ptr<IRenderingDriver> (*create_func)();
};

using GetDriverDescriptorFuncPtr = const DriverDesc* (*)();

}  // namespace app3d::rel
