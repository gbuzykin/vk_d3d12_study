#pragma once

#include "util/ref_ptr.h"

#include <uxs/db/value.h>

#include <limits>
#include <span>
#include <string_view>

#if defined(WIN32)
#    define APP3D_ENTRY_EXPORT extern "C" __declspec(dllexport)
#elif defined(__linux__)
#    define APP3D_ENTRY_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#define APP3D_REGISTER_RENDERING_DRIVER(name, driver_class) \
    constexpr app3d::rel::DriverDesc g_rendering_driver_descriptor{ \
        name, []() -> app3d::util::ref_ptr<app3d::rel::IRenderingDriver> { \
            return app3d::util::not_null{::new driver_class()}; \
        }}; \
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

enum class RenderTargetResult {
    SUCCESS = 0,
    SUBOPTIMAL,
    OUT_OF_DATE,
    FAILED,
};

struct Vec2i {
    std::int32_t x, y;
};

struct Vec3i {
    std::int32_t x, y, z;
};

struct Color4f {
    float r, g, b, a;
};

struct Extent2u {
    std::uint32_t width, height;
};

struct Extent3u {
    std::uint32_t width, height, depth;
};

struct Rect {
    Vec2i offset;
    Extent2u extent;
};

enum class PrimitiveTopology {
    POINTS = 0,
    LINES,
    LINE_STRIP,
    TRIANGLES,
    TRIANGLE_STRIP,
};

enum class BufferType {
    VERTEX = 0,
    CONSTANT,
};

struct IShaderModule {
    virtual ~IShaderModule() = default;
    virtual util::ref_counter& getRefCounter() = 0;
};

struct IPipelineLayout {
    virtual ~IPipelineLayout() = default;
    virtual util::ref_counter& getRefCounter() = 0;
};

struct IPipeline {
    virtual ~IPipeline() = default;
    virtual util::ref_counter& getRefCounter() = 0;
};

struct IBuffer {
    virtual ~IBuffer() = default;
    virtual util::ref_counter& getRefCounter() = 0;
    virtual bool updateVertexBuffer(std::span<const std::uint8_t> data, std::size_t offset) = 0;
    virtual bool updateConstantBuffer(std::span<const std::uint8_t> data, std::size_t offset) = 0;
};

struct IRenderTarget;
struct ITexture {
    virtual ~ITexture() = default;
    virtual util::ref_counter& getRefCounter() = 0;
    virtual bool updateTexture(std::span<const std::uint8_t> data, Vec3i offset, Extent3u extent) = 0;
    virtual util::ref_ptr<IRenderTarget> createRenderTarget(const uxs::db::value& opts) = 0;
};

struct ISampler {
    virtual ~ISampler() = default;
    virtual util::ref_counter& getRefCounter() = 0;
};

struct IDescriptorSet {
    virtual ~IDescriptorSet() = default;
    virtual util::ref_counter& getRefCounter() = 0;
    virtual void updateTextureSamplerDescriptor(ITexture& texture, ISampler& sampler, std::uint32_t slot) = 0;
    virtual void updateConstantBufferDescriptor(IBuffer& buffer, std::uint32_t slot) = 0;
};

struct IRenderTarget {
    virtual ~IRenderTarget() = default;
    virtual util::ref_counter& getRefCounter() = 0;
    virtual Extent2u getImageExtent() const = 0;
    virtual std::uint32_t getFifCount() const = 0;
    virtual RenderTargetResult beginRenderTarget(const Color4f& clear_color, float depth, std::uint32_t stencil) = 0;
    virtual bool endRenderTarget() = 0;
    virtual void setViewport(const Rect& rect, float z_near, float z_far) = 0;
    virtual void setScissor(const Rect& rect) = 0;
    virtual void bindPipeline(IPipeline& pipeline) = 0;
    virtual void bindVertexBuffer(IBuffer& buffer, std::size_t offset, std::uint32_t slot) = 0;
    virtual void bindDescriptorSet(IDescriptorSet& descriptor_set, std::uint32_t set_index) = 0;
    virtual void setPrimitiveTopology(PrimitiveTopology topology) = 0;
    virtual void drawGeometry(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex,
                              std::uint32_t first_instance) = 0;
};

struct IDevice {
    virtual ~IDevice() = default;
    virtual util::ref_counter& getRefCounter() = 0;
    virtual bool waitDevice() = 0;
    virtual util::ref_ptr<IShaderModule> createShaderModule(std::span<const std::uint32_t> source) = 0;
    virtual util::ref_ptr<IPipelineLayout> createPipelineLayout(const uxs::db::value& config) = 0;
    virtual util::ref_ptr<IPipeline> createPipeline(IRenderTarget& render_target, IPipelineLayout& pipeline_layout,
                                                    std::span<IShaderModule* const> shader_modules,
                                                    const uxs::db::value& config) = 0;
    virtual util::ref_ptr<IBuffer> createBuffer(std::size_t size, BufferType type) = 0;
    virtual util::ref_ptr<ITexture> createTexture(Extent3u extent) = 0;
    virtual util::ref_ptr<ISampler> createSampler() = 0;
    virtual util::ref_ptr<IDescriptorSet> createDescriptorSet(IPipelineLayout& pipeline_layout) = 0;
};

struct ISwapChain {
    virtual ~ISwapChain() = default;
    virtual util::ref_counter& getRefCounter() = 0;
    virtual bool recreateSwapChain(const uxs::db::value& opts) = 0;
    virtual util::ref_ptr<IRenderTarget> createRenderTarget(const uxs::db::value& opts) = 0;
};

struct ISurface {
    virtual ~ISurface() = default;
    virtual util::ref_counter& getRefCounter() = 0;
    virtual util::ref_ptr<ISwapChain> createSwapChain(IDevice& device, const uxs::db::value& opts) = 0;
};

struct WindowDescriptor {
    PlatformType platform;
    struct HandlePlaceholder {
        alignas(std::alignment_of_v<void*>) std::uint8_t handle[2 * sizeof(void*)];
    } handle;
};

struct IRenderingDriver {
    virtual ~IRenderingDriver() = default;
    virtual util::ref_counter& getRefCounter() = 0;
    virtual bool init(const uxs::db::value& app_info) = 0;
    virtual std::uint32_t getPhysicalDeviceCount() const = 0;
    virtual const char* getPhysicalDeviceName(std::uint32_t device_index) const = 0;
    virtual bool isSuitablePhysicalDevice(std::uint32_t device_index, const uxs::db::value& caps) const = 0;
    virtual util::ref_ptr<ISurface> createSurface(const WindowDescriptor& win_desc) = 0;
    virtual util::ref_ptr<IDevice> createDevice(std::uint32_t device_index, const uxs::db::value& caps) = 0;
};

// Registered rendering driver descriptor
struct DriverDesc {
    std::string_view name;
    util::ref_ptr<IRenderingDriver> (*create_func)();
};

using GetDriverDescriptorFuncPtr = const DriverDesc* (*)();

}  // namespace app3d::rel

namespace app3d::util {

template<typename T>
concept has_get_ref_counter_method = requires(T p) { p.getRefCounter(); };

template<typename T>
    requires(!has_ref_method<T> && has_get_ref_counter_method<T>)
struct ref_inc<T> {
    void operator()(T& p) const { p.getRefCounter().ref(); }
};

template<typename T>
    requires(!has_unref_method<T> && has_get_ref_counter_method<T>)
struct ref_dec<T> {
    void operator()(T& p) const { p.getRefCounter().unref(); }
};

}  // namespace app3d::util
