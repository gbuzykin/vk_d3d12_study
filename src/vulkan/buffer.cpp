#include "buffer.h"

#include "device.h"
#include "vulkan_logger.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Buffer class implementation

Buffer::Buffer(Device& device) : device_(device) {}

Buffer::~Buffer() { vmaDestroyBuffer(device_.get().getAllocator(), buffer_, allocation_); }

bool Buffer::create(VkDeviceSize size, VkBufferUsageFlags usage) {
    const VkBufferCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    const VmaAllocationCreateInfo alloc_info{
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VkResult result = vmaCreateBuffer(device_.get().getAllocator(), &create_info, &alloc_info, &buffer_, &allocation_,
                                      nullptr);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create buffer: {}", result);
        return false;
    }

    return true;
}

//@{ IBuffer

bool Buffer::updateVertexBuffer(std::span<const std::uint8_t> data, std::size_t offset) {
    return device_.get().updateBuffer(data.data(), VkDeviceSize(data.size()), buffer_, VkDeviceSize(offset),
                                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                                      VK_ACCESS_NONE, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, {});
}

bool Buffer::updateConstantBuffer(std::span<const std::uint8_t> data, std::size_t offset) {
    return device_.get().updateBuffer(data.data(), VkDeviceSize(data.size()), buffer_, VkDeviceSize(offset),
                                      VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                                      VK_ACCESS_NONE, VK_ACCESS_UNIFORM_READ_BIT, {});
}

//@}
