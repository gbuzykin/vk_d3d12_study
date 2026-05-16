#include "rel/tables.h"

#include <uxs/db/database_error.h>

#include <unordered_map>

using namespace app3d::rel;

namespace {
const std::unordered_map<std::string_view, Format> g_formats{
    {"FLOAT", Format::R32_FLOAT},           {"FLOAT2", Format::R32G32_FLOAT},  {"FLOAT3", Format::R32G32B32_FLOAT},
    {"FLOAT4", Format::R32G32B32A32_FLOAT}, {"BYTE4", Format::R8G8B8A8_UNORM},
};
const std::unordered_map<std::string_view, ShaderStage> g_shader_stages{
    {"ALL", ShaderStage::ALL_STAGES},
    {"VERTEX", ShaderStage::VERTEX_SHADER},
    {"PIXEL", ShaderStage::PIXEL_SHADER},
};
const std::unordered_map<std::string_view, PrimitiveTopology> g_primitive_topologies{
    {"POINTS", PrimitiveTopology::POINTS},
    {"LINES", PrimitiveTopology::LINES},
    {"LINE_STRIP", PrimitiveTopology::LINE_STRIP},
    {"TRIANGLES", PrimitiveTopology::TRIANGLES},
    {"TRIANGLE_STRIP", PrimitiveTopology::TRIANGLE_STRIP},
};
const std::unordered_map<std::string_view, DescriptorType> g_descriptor_types{
    {"SAMPLER", DescriptorType::SAMPLER},
    {"COMBINED_TEXTURE_SAMPLER", DescriptorType::COMBINED_TEXTURE_SAMPLER},
    {"TEXTURE", DescriptorType::TEXTURE},
    {"BUFFER", DescriptorType::BUFFER},
    {"RW_TEXTURE", DescriptorType::RW_TEXTURE},
    {"RW_BUFFER", DescriptorType::RW_BUFFER},
    {"CONSTANT_BUFFER", DescriptorType::CONSTANT_BUFFER},
    {"TEXTURE_BUFFER", DescriptorType::TEXTURE_BUFFER},
    {"STRUCTURED_BUFFER", DescriptorType::STRUCTURED_BUFFER},
    {"RW_STRUCTURED_BUFFER", DescriptorType::RW_STRUCTURED_BUFFER},
    {"CONSTANT_BUFFER_DYNAMIC", DescriptorType::CONSTANT_BUFFER_DYNAMIC},
    {"TEXTURE_BUFFER_DYNAMIC", DescriptorType::TEXTURE_BUFFER_DYNAMIC},
    {"STRUCTURED_BUFFER_DYNAMIC", DescriptorType::STRUCTURED_BUFFER_DYNAMIC},
    {"RW_STRUCTURED_BUFFER_DYNAMIC", DescriptorType::RW_STRUCTURED_BUFFER_DYNAMIC},
};
}  // namespace

Format app3d::rel::parseFormat(std::string_view fmt) {
    auto it = g_formats.find(fmt);
    if (it != g_formats.end()) { return it->second; }
    throw uxs::db::database_error("unknown format");
}

ShaderStage app3d::rel::parseShaderStage(std::string_view stage) {
    auto it = g_shader_stages.find(stage);
    if (it != g_shader_stages.end()) { return it->second; }
    throw uxs::db::database_error("unknown shader stage");
}

PrimitiveTopology app3d::rel::parsePrimitiveTopology(std::string_view topology) {
    auto it = g_primitive_topologies.find(topology);
    if (it != g_primitive_topologies.end()) { return it->second; }
    throw uxs::db::database_error("unknown primitive topology");
}

DescriptorType app3d::rel::parseDescriptorType(std::string_view type) {
    auto it = g_descriptor_types.find(type);
    if (it != g_descriptor_types.end()) { return it->second; }
    throw uxs::db::database_error("unknown descriptor type");
}
