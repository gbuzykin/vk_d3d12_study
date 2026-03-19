#include "vulkan_api.h"

namespace app3d::rel::vulkan {
#define EXPORTED_VK_FUNCTION(name)                                 PFN_##name name
#define GLOBAL_LEVEL_VK_FUNCTION(name)                             PFN_##name name
#define INSTANCE_LEVEL_VK_FUNCTION(name)                           PFN_##name name
#define INSTANCE_LEVEL_VK_FUNCTION_FROM_EXTENSION(name, extension) PFN_##name name
#define DEVICE_LEVEL_VK_FUNCTION(name)                             PFN_##name name
#define DEVICE_LEVEL_VK_FUNCTION_FROM_EXTENSION(name, extension)   PFN_##name name
#include "vulkan_function_list.inl"
}  // namespace app3d::rel::vulkan
