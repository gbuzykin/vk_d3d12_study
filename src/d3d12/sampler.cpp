#include "sampler.h"

#include "d3d12_logger.h"
#include "device.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// Sampler class implementation

Sampler::Sampler(Device& device) : device_(util::not_null{&device}) {}

Sampler::~Sampler() {}

bool Sampler::create(const SamplerOpts& opts) { return true; }

//@{ ISampler

//@}
