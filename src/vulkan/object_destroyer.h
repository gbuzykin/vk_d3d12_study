#pragma once

#include "vulkan_api.h"

namespace app3d::rel::vulkan {

template<typename Ty>
class ObjectDestroyer;

#define IMPLEMENT_VK_DESTROYER(type) \
    template<> \
    class ObjectDestroyer<Vk##type> { \
     public: \
        static void destroy(Vk##type obj) { \
            if (obj != VK_NULL_HANDLE) { vkDestroy##type(obj, nullptr); } \
        } \
    }

IMPLEMENT_VK_DESTROYER(Instance);
IMPLEMENT_VK_DESTROYER(Device);

#undef IMPLEMENT_VK_DESTROYER

#define IMPLEMENT_VK_DESTROYER_WITH_PARENT(parent_type, type) \
    template<> \
    class ObjectDestroyer<Vk##type> { \
     public: \
        explicit ObjectDestroyer(Vk##parent_type parent, Vk##type obj = VK_NULL_HANDLE) \
            : parent_(parent), obj_(obj) {} \
        ~ObjectDestroyer() { destroy(parent_, obj_); } \
        static void destroy(Vk##parent_type parent, Vk##type obj) { \
            if (obj != VK_NULL_HANDLE) { vkDestroy##type(parent, obj, nullptr); } \
        } \
        Vk##type& operator~() { return obj_; } \
\
     private: \
        Vk##parent_type parent_; \
        Vk##type obj_; \
    }

IMPLEMENT_VK_DESTROYER_WITH_PARENT(Instance, SurfaceKHR);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(Device, SwapchainKHR);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(Device, ImageView);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(Device, Semaphore);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(Device, Fence);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(Device, CommandPool);

#undef IMPLEMENT_VK_DESTROYER_WITH_PARENT

}  // namespace app3d::rel::vulkan
