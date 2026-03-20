#include "shader_module.h"

#include "device.h"
#include "object_destroyer.h"
#include "vulkan_logger.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// ShaderModule class implementation

ShaderModule::ShaderModule(Device& device) : device_(device) {}

ShaderModule::~ShaderModule() { ObjectDestroyer<VkShaderModule>::destroy(~device_, shader_module_); }

bool ShaderModule::create(std::span<const std::uint32_t> source) {
    const VkShaderModuleCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = source.size() * sizeof(std::uint32_t),
        .pCode = source.data(),
    };

    VkResult result = vkCreateShaderModule(~device_, &create_info, nullptr, &shader_module_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create shader module: {}", result);
        return false;
    }

    return true;
}

//@{ IShaderModule

//@}
