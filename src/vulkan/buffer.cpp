#include "buffer.h"

#include "device.h"
#include "rendering_driver.h"
#include "tables.h"
#include "vulkan_logger.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Buffer class implementation

Buffer::Buffer(Device& device) : device_(util::not_null{&device}) {}

Buffer::~Buffer() { vmaDestroyBuffer(device_->getAllocator(), buffer_, allocation_); }

bool Buffer::create(BufferType type, VkDeviceSize size) {
    if (type == BufferType::CONSTANT) {
        const auto& props = device_->getPhysicalDevice().getProperties();
        alignment_ = props.limits.minUniformBufferOffsetAlignment;
        size = (size + alignment_ - 1) & ~(alignment_ - 1);
    }

    const VkBufferCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VkBufferUsageFlags(TBL_VK_BUFFER_USAGE[unsigned(type)] | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkResult result = vmaCreateBuffer(device_->getAllocator(), &create_info,
                                      constAddressOf(VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_AUTO}), &buffer_,
                                      &allocation_, nullptr);
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
            return device_->updateBuffer(data, buffer_, VkDeviceSize(offset), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_NONE,
                                         VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, {});
        } break;
        case BufferType::CONSTANT: {
            return device_->updateBuffer(data, buffer_, VkDeviceSize(offset), VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_NONE,
                                         VK_ACCESS_UNIFORM_READ_BIT, {});
        } break;
        default: return false;
    }
}

//@}
