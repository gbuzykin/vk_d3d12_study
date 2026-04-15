#include "pipeline.h"

#include "d3d12_logger.h"
#include "device.h"
#include "pipeline_layout.h"
#include "render_target.h"
#include "shader_module.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// Pipeline class implementation

Pipeline::Pipeline(Device& device, RenderTarget& render_target, PipelineLayout& pipeline_layout)
    : device_(util::not_null{&device}), render_target_(util::not_null{&render_target}),
      pipeline_layout_(util::not_null{&pipeline_layout}) {}

Pipeline::~Pipeline() {}

bool Pipeline::create(std::span<IShaderModule* const> shader_modules, const uxs::db::value& config) { return true; }

//@{ IPipeline

//@}
