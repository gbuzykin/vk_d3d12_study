#include "descriptor_set.h"

#include "device.h"
#include "pipeline_layout.h"
#include "sampler.h"
#include "texture.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// DescriptorSet class implementation

DescriptorSet::DescriptorSet(Device& device, PipelineLayout& pipeline_layout)
    : device_(util::not_null{&device}), pipeline_layout_(util::not_null{&pipeline_layout}) {}

DescriptorSet::~DescriptorSet() {}

bool DescriptorSet::create() { return true; }

//@{ IDescriptorSet

void DescriptorSet::updateTextureSamplerDescriptor(ITexture& texture, ISampler& sampler, std::uint32_t slot) {}

void DescriptorSet::updateConstantBufferDescriptor(IBuffer& buffer, std::uint32_t slot) {}

//@}
