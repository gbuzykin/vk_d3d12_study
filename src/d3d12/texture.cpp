#include "texture.h"

#include "d3d12_logger.h"
#include "device.h"
#include "render_target.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// Texture class implementation

Texture::Texture(Device& device) : device_(util::not_null{&device}) {}

Texture::~Texture() {}

bool Texture::create(const TextureOpts& opts) { return true; }

//@{ ITexture

bool Texture::updateTexture(std::span<const std::uint8_t> data, Vec3i offset, Extent3u extent) { return true; }

util::ref_ptr<IRenderTarget> Texture::createRenderTarget(const uxs::db::value& opts) {
    auto render_target = util::make_new<RenderTarget>(*device_, *this);
    if (!render_target->create(opts)) { return nullptr; }
    return std::move(render_target);
}

//@}
