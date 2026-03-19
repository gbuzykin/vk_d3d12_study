#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

#include <cstddef>

namespace app3d::rel::vulkan {

class Device;

class Buffer : public IBuffer {
 public:
    explicit Buffer(Device& device);
    ~Buffer() override;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    bool create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlagBits desired_properties);

    VkBuffer operator~() { return buffer_; }
    VkDeviceMemory getMemoryObject() { return memory_object_; }

    //@{ IBuffer
    bool updateBuffer(std::span<const std::uint8_t> data, std::size_t offset) override;
    //@}

 private:
    Device& device_;
    VkBuffer buffer_{VK_NULL_HANDLE};
    VkDeviceMemory memory_object_{VK_NULL_HANDLE};

    bool allocateAndBindMemoryObjectToBuffer(VkMemoryPropertyFlagBits desired_properties);
};

}  // namespace app3d::rel::vulkan
