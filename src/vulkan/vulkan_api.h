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

struct InstanceVkFuncTable {
#define EXPORTED_VK_FUNCTION(name)                                        PFN_##name name;
#define GLOBAL_LEVEL_VK_FUNCTION(name)                                    PFN_##name name;
#define INSTANCE_LEVEL_VK_FUNCTION(name)                                  PFN_##name name;
#define INSTANCE_LEVEL_VK_FUNCTION_PHYDEV(name)                           PFN_##name name;
#define INSTANCE_LEVEL_VK_FUNCTION_DEV(name)                              PFN_##name name;
#define INSTANCE_LEVEL_VK_FUNCTION_FROM_EXTENSION(name, extension)        PFN_##name name;
#define INSTANCE_LEVEL_VK_FUNCTION_FROM_EXTENSION_PHYDEV(name, extension) PFN_##name name;
#include "vulkan_function_list.inl"
};

struct DeviceVkFuncTable {
#define DEVICE_LEVEL_VK_FUNCTION(name)                                 PFN_##name name;
#define DEVICE_LEVEL_VK_FUNCTION_QUEUE(name)                           PFN_##name name;
#define DEVICE_LEVEL_VK_FUNCTION_CMD(name)                             PFN_##name name;
#define DEVICE_LEVEL_VK_FUNCTION_FROM_EXTENSION(name, extension)       PFN_##name name;
#define DEVICE_LEVEL_VK_FUNCTION_FROM_EXTENSION_QUEUE(name, extension) PFN_##name name;
#define DEVICE_LEVEL_VK_FUNCTION_FROM_EXTENSION_CMD(name, extension)   PFN_##name name;
#include "vulkan_function_list.inl"
};

}  // namespace app3d::rel::vulkan
