#pragma once

#include "vulkan_api.h"

#include <string_view>

namespace app3d::rel::vulkan {
VkShaderStageFlagBits parseShaderStage(std::string_view stage);
VkFormat parseInputFormat(std::string_view fmt);
VkDescriptorType parseDescriptorType(std::string_view type);
}  // namespace app3d::rel::vulkan
