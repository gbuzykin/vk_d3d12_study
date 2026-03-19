#include "buffer.h"

#include "device.h"
#include "object_destroyer.h"
#include "rendering_driver.h"

#include "common/logger.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// Buffer class implementation

Buffer::Buffer(Device& device) : device_(device) {}

Buffer::~Buffer() {
    ObjectDestroyer<VkDeviceMemory>::destroy(~device_, memory_object_);
    ObjectDestroyer<VkBuffer>::destroy(~device_, buffer_);
}

bool Buffer::create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlagBits desired_properties) {
    const VkBufferCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkResult result = vkCreateBuffer(~device_, &create_info, nullptr, &buffer_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create a buffer");
        return false;
    }

    if (!allocateAndBindMemoryObjectToBuffer(desired_properties)) { return false; }

    return true;
}

//@{ IBuffer

bool Buffer::updateBuffer(std::span<const std::uint8_t> data, std::size_t offset) {
    return device_.writeToDeviceLocalMemory(VkDeviceSize(data.size()), data.data(), buffer_, VkDeviceSize(offset), 0,
                                            VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, {});
}

//@}

bool Buffer::allocateAndBindMemoryObjectToBuffer(VkMemoryPropertyFlagBits desired_properties) {
    VkMemoryRequirements memory_requirements{};
    vkGetBufferMemoryRequirements(~device_, buffer_, &memory_requirements);

    const auto& memory_properties = device_.getPhysicalDevice().getMemoryProperties();

    for (std::uint32_t type = 0; type < memory_properties.memoryTypeCount; ++type) {
        if ((memory_requirements.memoryTypeBits & (1U << type)) &&
            (memory_properties.memoryTypes[type].propertyFlags & desired_properties) == desired_properties) {
            const VkMemoryAllocateInfo allocate_info{
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = memory_requirements.size,
                .memoryTypeIndex = type,
            };

            VkResult result = vkAllocateMemory(~device_, &allocate_info, nullptr, &memory_object_);
            if (result == VK_SUCCESS) { break; }
        }
    }

    if (memory_object_ == VK_NULL_HANDLE) {
        logError(LOG_VK "couldn't allocate memory for a buffer");
        return false;
    }

    VkResult result = vkBindBufferMemory(~device_, buffer_, memory_object_, 0);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't bind memory object to a buffer");
        return false;
    }

    return true;
}
