#include "pipeline_layout.h"

#include "d3d12_logger.h"
#include "device.h"
#include "pipeline.h"
#include "render_target.h"
#include "shader_module.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// PipelineLayout class implementation

PipelineLayout::PipelineLayout(Device& device) : device_(util::not_null(&device)) {}

PipelineLayout::~PipelineLayout() {}

bool PipelineLayout::create(const uxs::db::value& config) { return true; }

//@{ IPipelineLayout

//@}
