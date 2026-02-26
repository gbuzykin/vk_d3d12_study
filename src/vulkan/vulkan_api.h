#pragma once

#define VK_NO_PROTOTYPES
#if defined(APP3D_USE_PLATFORM_WIN32)
#    define VK_USE_PLATFORM_WIN32_KHR
#elif defined(APP3D_USE_PLATFORM_XLIB)
#    define VK_USE_PLATFORM_XLIB_KHR
#elif defined(APP3D_USE_PLATFORM_XCB)
#    define VK_USE_PLATFORM_XCB_KHR
#elif defined(APP3D_USE_PLATFORM_WAYLAND)
#    define VK_USE_PLATFORM_WAYLAND_KHR
#else
#    error Unspecified platform
#endif

#include <vulkan/vulkan.h>

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
