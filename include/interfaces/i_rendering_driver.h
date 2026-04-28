#pragma once

#include "rel/data_blob.h"
#include "rel/win_desc.h"
#include "util/ref_counter.h"
#include "util/ref_ptr.h"

#include <uxs/db/value.h>
#include <uxs/utility.h>

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

enum class RenderTargetResult {
    SUCCESS = 0,
    SUBOPTIMAL,
    OUT_OF_DATE,
    FAILED,
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

enum class SamplerFilter {
    MIN_MAG_MIP_POINT = 0,
    MIN_MAG_POINT_MIP_LINEAR,
    MIN_POINT_MAG_LINEAR_MIP_POINT,
    MIN_POINT_MAG_MIP_LINEAR,
    MIN_LINEAR_MAG_MIP_POINT,
    MIN_LINEAR_MAG_POINT_MIP_LINEAR,
    MIN_MAG_LINEAR_MIP_POINT,
    MIN_MAG_MIP_LINEAR,
    ANISOTROPIC,
};

enum class SamplerAddressMode {
    REPEAT = 0,
    MIRRORED_REPEAT,
    CLAMP_TO_EDGE,
    MIRROR_CLAMP_TO_EDGE,
};

enum class TextureFlags {
    NONE = 0,
    RENDER_TARGET = 1,
};
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(TextureFlags);

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

struct UpdateTextureDesc {
    std::size_t buffer_offset;
    std::uint32_t buffer_row_size;
    std::uint32_t buffer_row_count;
    Vec3i image_offset;
    Extent3u image_extent;
};

struct SamplerDesc {
    SamplerFilter filter;
    SamplerAddressMode address_mode_u;
    SamplerAddressMode address_mode_v;
    SamplerAddressMode address_mode_w;
    float min_lod;
    float max_lod;
    float mip_lod_bias;
    std::uint32_t max_anisotropy;
};

struct TextureDesc {
    TextureFlags flags;
    Extent3u extent;
};

struct IShaderModule {
    virtual ~IShaderModule() = default;
    virtual util::ref_counter& getRefCounter() = 0;
};

struct IDescriptorSet;
struct IPipelineLayout {
    virtual ~IPipelineLayout() = default;
    virtual util::ref_counter& getRefCounter() = 0;
    virtual util::ref_ptr<IDescriptorSet> createDescriptorSet(std::uint32_t set_layout_index) = 0;
    virtual void resetDescriptorAllocator() = 0;
};

struct IPipeline {
    virtual ~IPipeline() = default;
    virtual util::ref_counter& getRefCounter() = 0;
};

struct IBuffer {
    virtual ~IBuffer() = default;
    virtual util::ref_counter& getRefCounter() = 0;
    virtual bool updateBuffer(std::span<const std::uint8_t> data, std::uint64_t offset) = 0;
};

struct IRenderTarget;
struct ITexture {
    virtual ~ITexture() = default;
    virtual util::ref_counter& getRefCounter() = 0;
    virtual bool updateTexture(const std::uint8_t* data, std::uint32_t first_subresource,
                               std::span<const UpdateTextureDesc> update_subresource_descs) = 0;
    virtual util::ref_ptr<IRenderTarget> createRenderTarget(const uxs::db::value& opts) = 0;
};

struct ISampler {
    virtual ~ISampler() = default;
    virtual util::ref_counter& getRefCounter() = 0;
};

struct IDescriptorSet {
    virtual ~IDescriptorSet() = default;
    virtual util::ref_counter& getRefCounter() = 0;
    virtual void updateCombinedTextureSamplerDescriptor(ITexture& texture, ISampler& sampler, std::uint32_t slot) = 0;
    virtual void updateConstantBufferDescriptor(IBuffer& buffer, std::uint32_t slot) = 0;
};

struct IRenderTarget {
    virtual ~IRenderTarget() = default;
    virtual util::ref_counter& getRefCounter() = 0;
    virtual Extent2u getImageExtent() const = 0;
    virtual std::uint32_t getFifCount() const = 0;
    virtual bool isInvertedNdcY() const = 0;
    virtual RenderTargetResult beginRenderTarget(const Color4f& clear_color, float depth, std::uint32_t stencil) = 0;
    virtual bool endRenderTarget() = 0;
    virtual void setViewport(const Rect& rect, float z_near, float z_far) = 0;
    virtual void setScissor(const Rect& rect) = 0;
    virtual void bindPipeline(IPipeline& pipeline) = 0;
    virtual void bindVertexBuffer(IBuffer& buffer, std::uint32_t slot, std::uint32_t stride, std::uint32_t offset) = 0;
    virtual void bindDescriptorSet(IDescriptorSet& descriptor_set, std::uint32_t set_index) = 0;
    virtual void setPrimitiveTopology(PrimitiveTopology topology) = 0;
    virtual void drawGeometry(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex,
                              std::uint32_t first_instance) = 0;
};

struct ISurface {
    virtual ~ISurface() = default;
    virtual util::ref_counter& getRefCounter() = 0;
};

struct ISwapChain {
    virtual ~ISwapChain() = default;
    virtual util::ref_counter& getRefCounter() = 0;
    virtual bool recreate(const uxs::db::value& opts) = 0;
    virtual util::ref_ptr<IRenderTarget> createRenderTarget(const uxs::db::value& opts) = 0;
};

struct IDevice {
    virtual ~IDevice() = default;
    virtual util::ref_counter& getRefCounter() = 0;
    virtual bool waitDevice() = 0;
    virtual util::ref_ptr<ISwapChain> createSwapChain(ISurface& surface, const uxs::db::value& opts) = 0;
    virtual util::ref_ptr<IShaderModule> createShaderModule(DataBlob bytecode) = 0;
    virtual util::ref_ptr<IPipelineLayout> createPipelineLayout(const uxs::db::value& config) = 0;
    virtual util::ref_ptr<IPipeline> createPipeline(IRenderTarget& render_target, IPipelineLayout& pipeline_layout,
                                                    std::span<IShaderModule* const> shader_modules,
                                                    const uxs::db::value& config) = 0;
    virtual util::ref_ptr<IBuffer> createBuffer(BufferType type, std::uint64_t size) = 0;
    virtual util::ref_ptr<ITexture> createTexture(const TextureDesc& desc) = 0;
    virtual util::ref_ptr<ISampler> createSampler(const SamplerDesc& desc) = 0;
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
    virtual DataBlob compileShader(const DataBlob& source_text, const uxs::db::value& args,
                                   DataBlob& compiler_output) = 0;
};

// Registered rendering driver descriptor
struct DriverDesc {
    std::string_view name;
    util::ref_ptr<IRenderingDriver> (*create_func)();
};

using GetDriverDescriptorFuncPtr = const DriverDesc* (*)();

}  // namespace app3d::rel
