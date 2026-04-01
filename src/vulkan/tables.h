#pragma once

#include "vulkan_api.h"

#include <cstdint>
#include <string_view>
#include <utility>

namespace app3d::rel::vulkan {
VkShaderStageFlagBits parseShaderStage(std::string_view stage);
VkFormat parseFormat(std::string_view fmt);
DescriptorType parseDescriptorType(std::string_view type);
std::pair<std::uint32_t, std::uint32_t> getFormatSizeAlignment(VkFormat fmt);
}  // namespace app3d::rel::vulkan
