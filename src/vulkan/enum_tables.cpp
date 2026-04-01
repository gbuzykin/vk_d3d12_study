#include "enum_tables.h"

#include <uxs/db/database_error.h>

#include <unordered_map>

namespace {
const std::unordered_map<std::string_view, VkShaderStageFlagBits> g_shader_stages{
    {"vertex", VK_SHADER_STAGE_VERTEX_BIT},
    {"pixel", VK_SHADER_STAGE_FRAGMENT_BIT},
};
const std::unordered_map<std::string_view, VkFormat> g_input_formats{
    {"float", VK_FORMAT_R32_SFLOAT},
    {"float2", VK_FORMAT_R32G32_SFLOAT},
    {"float3", VK_FORMAT_R32G32B32_SFLOAT},
    {"float4", VK_FORMAT_R32G32B32A32_SFLOAT},
};
const std::unordered_map<std::string_view, VkDescriptorType> g_descriptor_types{
    {"texture_sampler", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
    {"constant_buffer", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
};
}  // namespace

VkShaderStageFlagBits app3d::rel::vulkan::parseShaderStage(std::string_view stage) {
    auto it = g_shader_stages.find(stage);
    if (it != g_shader_stages.end()) { return it->second; }
    throw uxs::db::database_error("unknown shader stage");
}

VkFormat app3d::rel::vulkan::parseInputFormat(std::string_view fmt) {
    auto it = g_input_formats.find(fmt);
    if (it != g_input_formats.end()) { return it->second; }
    throw uxs::db::database_error("unknown input format");
}

VkDescriptorType app3d::rel::vulkan::parseDescriptorType(std::string_view type) {
    auto it = g_descriptor_types.find(type);
    if (it != g_descriptor_types.end()) { return it->second; }
    throw uxs::db::database_error("unknown descriptor type");
}
