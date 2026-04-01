#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::vulkan {

class Device;

class Buffer : public IBuffer {
 public:
    explicit Buffer(Device& device);
    ~Buffer() override;

    Buffer(Buffer&& buf) noexcept
        : device_(buf.device_), size_(buf.size_), buffer_(buf.buffer_), allocation_(buf.allocation_) {
        size_ = 0, buffer_ = VK_NULL_HANDLE, allocation_ = VK_NULL_HANDLE;
    }

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    VkDeviceSize getSize() const { return size_; }

    bool create(VkDeviceSize size, VkBufferUsageFlags usage, bool host_access = false);

    VkBuffer operator~() { return buffer_; }
    VmaAllocation getAllocation() { return allocation_; }

    //@{ IBuffer
    bool updateVertexBuffer(std::span<const std::uint8_t> data, std::size_t offset) override;
    bool updateConstantBuffer(std::span<const std::uint8_t> data, std::size_t offset) override;
    //@}

 private:
    Device& device_;
    VkDeviceSize size_ = 0;
    VkBuffer buffer_{VK_NULL_HANDLE};
    VmaAllocation allocation_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
