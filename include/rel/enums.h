#pragma once

#include <uxs/utility.h>

namespace app3d::rel {

enum class Format {
    R32_FLOAT = 0,
    R32G32_FLOAT,
    R32G32B32_FLOAT,
    R32G32B32A32_FLOAT,
    R8G8B8A8_UNORM,
    TOTAL_COUNT,
};

enum class PrimitiveTopology {
    POINTS = 0,
    LINES,
    LINE_STRIP,
    TRIANGLES,
    TRIANGLE_STRIP,
    TOTAL_COUNT,
};

enum class BufferType {
    VERTEX = 0,
    CONSTANT,
    TOTAL_COUNT,
};

enum class ShaderStage {
    ALL_STAGES = 0,
    VERTEX_SHADER,
    PIXEL_SHADER,
    TOTAL_COUNT,
};

enum class DescriptorType {
    COMBINED_TEXTURE_SAMPLER = 0,
    CONSTANT_BUFFER,
    TOTAL_COUNT,
};

enum class BindingType {
    CONSTANT_BUFFER = 0,
    SHADER_RESOURCE,
    UNORDERED,
    SAMPLER,
    TOTAL_COUNT,
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
    TOTAL_COUNT,
};

enum class SamplerAddressMode {
    REPEAT = 0,
    MIRRORED_REPEAT,
    CLAMP_TO_EDGE,
    MIRROR_CLAMP_TO_EDGE,
    TOTAL_COUNT,
};

enum class TextureFlags {
    NONE = 0,
    RENDER_TARGET = 1,
};
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(TextureFlags);

}  // namespace app3d::rel
