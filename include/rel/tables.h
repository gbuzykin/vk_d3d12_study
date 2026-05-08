#pragma once

#include "rel/config.h"
#include "rel/enums.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace app3d::rel {

constexpr std::array TBL_FORMAT_SIZE{
    // Format::
    std::uint32_t(4),   // R32_FLOAT
    std::uint32_t(8),   // R32G32_FLOAT
    std::uint32_t(12),  // R32G32B32_FLOAT
    std::uint32_t(16),  // R32G32B32A32_FLOAT
    std::uint32_t(4),   // R8G8B8A8_UNORM
};

constexpr std::array TBL_FORMAT_ALIGNMENT{
    // Format::
    std::uint32_t(4),  // R32_FLOAT
    std::uint32_t(4),  // R32G32_FLOAT
    std::uint32_t(4),  // R32G32B32_FLOAT
    std::uint32_t(4),  // R32G32B32A32_FLOAT
    std::uint32_t(4),  // R8G8B8A8_UNORM
};

constexpr std::array TBL_DESC_BINDING_TYPE{
    // DescriptorType::
    BindingType::SHADER_RESOURCE,  // COMBINED_TEXTURE_SAMPLER
    BindingType::CONSTANT_BUFFER,  // CONSTANT_BUFFER
    BindingType::CONSTANT_BUFFER,  // CONSTANT_BUFFER_DYNAMIC
};

APP3D_REL_EXPORT Format parseFormat(std::string_view fmt);
APP3D_REL_EXPORT ShaderStage parseShaderStage(std::string_view stage);
APP3D_REL_EXPORT PrimitiveTopology parsePrimitiveTopology(std::string_view topology);
APP3D_REL_EXPORT DescriptorType parseDescriptorType(std::string_view type);

}  // namespace app3d::rel
