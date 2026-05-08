#include "surface.h"

#include "device.h"
#include "rendering_driver.h"
#include "swap_chain.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// Surface class implementation

Surface::Surface(RenderingDriver& instance) : instance_(util::not_null{&instance}) {}

Surface::~Surface() {}

bool Surface::create(const WindowDescriptor& win_desc) {
    win_desc_ = win_desc;
    return true;
}

//@{ ISurface

//@}
