#include "shader_module.h"

#include "device.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// ShaderModule class implementation

ShaderModule::ShaderModule(Device& device) : device_(util::not_null{&device}) {}

ShaderModule::~ShaderModule() {}

//@{ IShaderModule

//@}
