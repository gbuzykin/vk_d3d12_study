#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#ifdef min
#    undef min
#endif

#ifdef max
#    undef max
#endif

namespace app3d::rel::vulkan {
#define EXPORTED_VK_FUNCTION(name)                                 extern PFN_##name name
#define GLOBAL_LEVEL_VK_FUNCTION(name)                             extern PFN_##name name
#define INSTANCE_LEVEL_VK_FUNCTION(name)                           extern PFN_##name name
#define INSTANCE_LEVEL_VK_FUNCTION_FROM_EXTENSION(name, extension) extern PFN_##name name
#define DEVICE_LEVEL_VK_FUNCTION(name)                             extern PFN_##name name
#define DEVICE_LEVEL_VK_FUNCTION_FROM_EXTENSION(name, extension)   extern PFN_##name name
#include "vulkan_function_list.inl"
}  // namespace app3d::rel::vulkan
