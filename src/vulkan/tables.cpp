#include "tables.h"

#include "vulkan_logger.h"

#include <uxs/db/database_error.h>

#include <unordered_map>

namespace {
const std::unordered_map<std::string_view, VkShaderStageFlagBits> g_shader_stages{
    {"ALL", VK_SHADER_STAGE_ALL_GRAPHICS},
    {"VERTEX", VK_SHADER_STAGE_VERTEX_BIT},
    {"PIXEL", VK_SHADER_STAGE_FRAGMENT_BIT},
};
const std::unordered_map<std::string_view, VkFormat> g_formats{
    {"FLOAT", VK_FORMAT_R32_SFLOAT},        {"FLOAT2", VK_FORMAT_R32G32_SFLOAT},
    {"FLOAT3", VK_FORMAT_R32G32B32_SFLOAT}, {"FLOAT4", VK_FORMAT_R32G32B32A32_SFLOAT},
    {"BYTE4", VK_FORMAT_R8G8B8A8_UNORM},
};
const std::unordered_map<std::string_view, VkDescriptorType> g_descriptor_types{
    {"COMBINED_TEXTURE_SAMPLER", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
    {"CONSTANT_BUFFER", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
};
const std::unordered_map<VkFormat, std::pair<std::uint32_t, std::uint32_t>> g_format_size_alignment{
    {VK_FORMAT_R32_SFLOAT, {4, 4}},           {VK_FORMAT_R32G32_SFLOAT, {8, 4}},  {VK_FORMAT_R32G32B32_SFLOAT, {12, 4}},
    {VK_FORMAT_R32G32B32A32_SFLOAT, {16, 4}}, {VK_FORMAT_R8G8B8A8_UNORM, {4, 4}},
};
}  // namespace

VkShaderStageFlagBits app3d::rel::vulkan::parseShaderStage(std::string_view stage) {
    auto it = g_shader_stages.find(stage);
    if (it != g_shader_stages.end()) { return it->second; }
    throw uxs::db::database_error("unknown shader stage");
}

VkFormat app3d::rel::vulkan::parseFormat(std::string_view fmt) {
    auto it = g_formats.find(fmt);
    if (it != g_formats.end()) { return it->second; }
    throw uxs::db::database_error("unknown format");
}

VkDescriptorType app3d::rel::vulkan::parseDescriptorType(std::string_view type) {
    auto it = g_descriptor_types.find(type);
    if (it != g_descriptor_types.end()) { return it->second; }
    throw uxs::db::database_error("unknown descriptor type");
}

std::pair<std::uint32_t, std::uint32_t> app3d::rel::vulkan::getFormatSizeAlignment(VkFormat fmt) {
    auto it = g_format_size_alignment.find(fmt);
    if (it != g_format_size_alignment.end()) { return it->second; }
    logError(LOG_VK "unknown format");
    return {0, 0};
}
