#pragma once

#include "enums.h"

#include <cstddef>
#include <cstdint>

namespace app3d::rel {

struct Vec2i {
    std::int32_t x, y;
};

struct Vec3i {
    std::int32_t x, y, z;
    explicit constexpr operator Vec2i() const { return {.x = x, .y = y}; }
};

struct Color4f {
    float r, g, b, a;
};

struct Extent2u {
    std::uint32_t width, height;
};

struct Extent3u {
    std::uint32_t width, height, depth;
    explicit constexpr operator Extent2u() const { return {.width = width, .height = height}; }
};

struct Rect {
    Vec2i offset;
    Extent2u extent;
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
    Format format;
    Extent3u extent;
    TextureFlags flags;
};

struct UpdateTextureDesc {
    std::size_t buffer_offset;
    std::uint32_t buffer_row_size;
    std::uint32_t buffer_row_count;
    Vec3i image_offset;
    Extent3u image_extent;
};

}  // namespace app3d::rel
