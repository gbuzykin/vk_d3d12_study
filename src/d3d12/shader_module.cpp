#include "shader_module.h"

#include "d3d12_logger.h"
#include "device.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// ShaderModule class implementation

ShaderModule::ShaderModule(Device& device) : device_(util::not_null{&device}) {}

ShaderModule::~ShaderModule() {}

bool ShaderModule::create(std::span<const std::uint32_t> source) { return true; }

//@{ IShaderModule

//@}
