#include "buffer.h"

#include "device.h"
#include "rendering_driver.h"
#include "vulkan_logger.h"

#include <array>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Buffer class implementation

Buffer::Buffer(Device& device) : device_(device) {}

Buffer::~Buffer() { vmaDestroyBuffer(device_.getAllocator(), buffer_, allocation_); }

bool Buffer::create(BufferType type, VkDeviceSize size, bool host_access) {
    constexpr std::array usage{
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    };

    const VkBufferCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = host_access ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT :
                               VkBufferUsageFlags(usage[unsigned(type)] | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    const VmaAllocationCreateInfo alloc_info{
        .flags = VmaAllocationCreateFlags(host_access ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT : 0),
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    if (buffer_ != VK_NULL_HANDLE) {
        vmaDestroyBuffer(device_.getAllocator(), buffer_, allocation_);
        buffer_ = VK_NULL_HANDLE;
        allocation_ = VK_NULL_HANDLE;
    }

    VkResult result = vmaCreateBuffer(device_.getAllocator(), &create_info, &alloc_info, &buffer_, &allocation_,
                                      nullptr);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create buffer: {}", result);
        return false;
    }

    type_ = type;
    size_ = size;
    return true;
}

//@{ IBuffer

bool Buffer::updateBuffer(std::span<const std::uint8_t> data, std::uint64_t offset) {
    switch (type_) {
        case BufferType::VERTEX: {
            return device_.updateBuffer(data.data(), VkDeviceSize(data.size()), buffer_, VkDeviceSize(offset),
                                        VK_ACCESS_NONE, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, {});
        } break;
        case BufferType::CONSTANT: {
            return device_.updateBuffer(data.data(), VkDeviceSize(data.size()), buffer_, VkDeviceSize(offset),
                                        VK_ACCESS_NONE, VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                                        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, {});
        } break;
        default: return false;
    }
}

//@}
