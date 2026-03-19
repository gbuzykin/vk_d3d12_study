#pragma once

#include "vulkan_api.h"

namespace app3d::rel::vulkan {

template<typename Ty>
class ObjectDestroyer;

#define IMPLEMENT_VK_DESTROYER(type, destroy_func) \
    template<> \
    class ObjectDestroyer<type> { \
     public: \
        static void destroy(type obj) { \
            if (obj != VK_NULL_HANDLE) { destroy_func(obj, nullptr); } \
        } \
    }

IMPLEMENT_VK_DESTROYER(VkInstance, vkDestroyInstance);
IMPLEMENT_VK_DESTROYER(VkDevice, vkDestroyDevice);

#undef IMPLEMENT_VK_DESTROYER

#define IMPLEMENT_VK_DESTROYER_WITH_PARENT(parent_type, type, destroy_func) \
    template<> \
    class ObjectDestroyer<type> { \
     public: \
        explicit ObjectDestroyer(parent_type parent, type obj = VK_NULL_HANDLE) : parent_(parent), obj_(obj) {} \
        ~ObjectDestroyer() { destroy(parent_, obj_); } \
        static void destroy(parent_type parent, type obj) { \
            if (obj != VK_NULL_HANDLE) { destroy_func(parent, obj, nullptr); } \
        } \
        type& operator~() { return obj_; } \
\
     private: \
        parent_type parent_; \
        type obj_; \
    }

IMPLEMENT_VK_DESTROYER_WITH_PARENT(VkInstance, VkSurfaceKHR, vkDestroySurfaceKHR);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(VkDevice, VkSwapchainKHR, vkDestroySwapchainKHR);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(VkDevice, VkImage, vkDestroyImage);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(VkDevice, VkImageView, vkDestroyImageView);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(VkDevice, VkSemaphore, vkDestroySemaphore);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(VkDevice, VkFence, vkDestroyFence);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(VkDevice, VkCommandPool, vkDestroyCommandPool);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(VkDevice, VkRenderPass, vkDestroyRenderPass);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(VkDevice, VkFramebuffer, vkDestroyFramebuffer);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(VkDevice, VkShaderModule, vkDestroyShaderModule);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(VkDevice, VkPipelineLayout, vkDestroyPipelineLayout);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(VkDevice, VkPipeline, vkDestroyPipeline);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(VkDevice, VkBuffer, vkDestroyBuffer);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(VkDevice, VkDeviceMemory, vkFreeMemory);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(VkDevice, VkSampler, vkDestroySampler);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(VkDevice, VkDescriptorPool, vkDestroyDescriptorPool);
IMPLEMENT_VK_DESTROYER_WITH_PARENT(VkDevice, VkDescriptorSetLayout, vkDestroyDescriptorSetLayout);

#undef IMPLEMENT_VK_DESTROYER_WITH_PARENT

}  // namespace app3d::rel::vulkan
