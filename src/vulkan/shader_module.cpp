#include "shader_module.h"

#include "device.h"
#include "object_destroyer.h"

#include "utils/logger.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::vulkan;

// --------------------------------------------------------
// ShaderModule class implementation

ShaderModule::ShaderModule(Device& device) : device_(device) {}

ShaderModule::~ShaderModule() { ObjectDestroyer<VkShaderModule>::destroy(~device_, shader_module_); }

bool ShaderModule::create(std::span<const std::uint8_t> source_spirv, const uxs::db::value& create_info) {
    const VkShaderModuleCreateInfo shader_module_create_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = source_spirv.size(),
        .pCode = reinterpret_cast<const std::uint32_t*>(source_spirv.data()),
    };

    VkResult result = vkCreateShaderModule(~device_, &shader_module_create_info, nullptr, &shader_module_);
    if (result != VK_SUCCESS) {
        logError(LOG_VK "couldn't create a shader module");
        return false;
    }

    return true;
}

//@{ IShaderModule

//@}
