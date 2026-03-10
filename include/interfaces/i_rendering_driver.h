#pragma once

#include <uxs/db/value.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <span>
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

enum class ResultCode { SUCCESS = 0, FAILED, OUT_OF_DATE };

template<typename Ty>
struct Vec2 {
    Ty x, y;
};

template<typename Ty>
struct Vec3 {
    Ty x, y, z;
};

template<typename Ty>
struct Vec4 {
    Ty x, y, z, w;
};

using Vec2i = Vec2<std::int32_t>;
using Vec2u = Vec2<std::uint32_t>;
using Vec2f = Vec2<float>;

using Vec3i = Vec3<std::int32_t>;
using Vec3u = Vec3<std::uint32_t>;
using Vec3f = Vec3<float>;

using Vec4i = Vec4<std::int32_t>;
using Vec4u = Vec4<std::uint32_t>;
using Vec4f = Vec4<float>;

template<typename Ty>
struct Extent2 {
    Ty width, height;
};

template<typename Ty>
struct Extent3 {
    Ty width, height, depth;
};

using Extent2i = Extent2<std::int32_t>;
using Extent2u = Extent2<std::uint32_t>;
using Extent2f = Extent2<float>;

using Extent3i = Extent2<std::int32_t>;
using Extent3u = Extent2<std::uint32_t>;
using Extent3f = Extent2<float>;

struct Rect {
    Vec2i offset;
    Extent2u extent;
};

struct IShaderModule {
    virtual ~IShaderModule() = default;
};

struct IPipeline {
    virtual ~IPipeline() = default;
};

struct IBuffer {
    virtual ~IBuffer() = default;
    virtual ResultCode updateBuffer(std::size_t size, const void* data, std::size_t offset) = 0;
};

struct IRenderTarget {
    virtual ~IRenderTarget() = default;
    virtual ResultCode beginRenderTarget(const Vec4f& clear_color) = 0;
    virtual ResultCode endRenderTarget() = 0;
    virtual void setViewport(const Rect& rect, float z_near, float z_far) = 0;
    virtual void setScissor(const Rect& rect) = 0;
    virtual void bindPipeline(IPipeline& pipeline) = 0;
    virtual void bindVertexBuffer(std::uint32_t first_binding, IBuffer& buffer, std::size_t offset) = 0;
    virtual void drawGeometry(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex,
                              std::uint32_t first_instance) = 0;
};

struct IDevice {
    virtual ~IDevice() = default;
    virtual IShaderModule* createShaderModule(std::span<const std::uint8_t> source_spirv,
                                              const uxs::db::value& create_info = {}) = 0;
    virtual IPipeline* createPipeline(IRenderTarget& render_target, std::span<IShaderModule* const> shader_modules,
                                      const uxs::db::value& create_info = {}) = 0;
    virtual IBuffer* createBuffer(std::size_t size) = 0;
};

struct ISwapChain {
    virtual ~ISwapChain() = default;
    virtual Extent2u getImageExtent() const = 0;
    virtual IRenderTarget* createRenderTarget(const uxs::db::value& create_info = {}) = 0;
};

struct ISurface {
    virtual ~ISurface() = default;
    virtual ISwapChain* createSwapChain(IDevice& device, const uxs::db::value& create_info = {}) = 0;
};

struct WindowHandle {
    alignas(std::alignment_of_v<void*>) std::uint8_t v[2 * sizeof(void*)];
};

struct IRenderingDriver {
    virtual ~IRenderingDriver() = default;
    virtual ResultCode init(const uxs::db::value& app_info) = 0;
    virtual std::uint32_t getPhysicalDeviceCount() const = 0;
    virtual const char* getPhysicalDeviceName(std::uint32_t device_index) const = 0;
    virtual bool isSuitablePhysicalDevice(std::uint32_t device_index, const uxs::db::value& caps) const = 0;
    virtual ISurface* createSurface(const WindowHandle& window_handle) = 0;
    virtual IDevice* createDevice(std::uint32_t device_index, const uxs::db::value& caps) = 0;
};

// Registered rendering driver descriptor
struct DriverDesc {
    std::string_view name;
    std::unique_ptr<IRenderingDriver> (*create_func)();
};

using GetDriverDescriptorFuncPtr = const DriverDesc* (*)();

}  // namespace app3d::rel
