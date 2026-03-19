#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#ifdef min
#    undef min
#endif

#ifdef max
#    undef max
#endif

#define LOG_VK "vulkan: "

namespace app3d::rel::vulkan {
#define EXPORTED_VK_FUNCTION(name)                                 extern PFN_##name name
#define GLOBAL_LEVEL_VK_FUNCTION(name)                             extern PFN_##name name
#define INSTANCE_LEVEL_VK_FUNCTION(name)                           extern PFN_##name name
#define INSTANCE_LEVEL_VK_FUNCTION_FROM_EXTENSION(name, extension) extern PFN_##name name
#define DEVICE_LEVEL_VK_FUNCTION(name)                             extern PFN_##name name
#define DEVICE_LEVEL_VK_FUNCTION_FROM_EXTENSION(name, extension)   extern PFN_##name name
#include "vulkan_function_list.inl"
}  // namespace app3d::rel::vulkan
