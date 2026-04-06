#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::vulkan {

class Device;

class Buffer : public util::ref_counter, public IBuffer {
 public:
    explicit Buffer(Device& device);
    ~Buffer() override;

    bool create(VkDeviceSize size, VkBufferUsageFlags usage);

    VkBuffer operator~() { return buffer_; }

    //@{ IBuffer
    util::ref_counter& getRefCounter() override { return *this; }
    bool updateVertexBuffer(std::span<const std::uint8_t> data, std::size_t offset) override;
    bool updateConstantBuffer(std::span<const std::uint8_t> data, std::size_t offset) override;
    //@}

 private:
    util::ref_ptr<Device> device_;
    VkBuffer buffer_{VK_NULL_HANDLE};
    VmaAllocation allocation_{VK_NULL_HANDLE};
};

}  // namespace app3d::rel::vulkan
