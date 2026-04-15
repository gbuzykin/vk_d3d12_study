#include "buffer.h"

#include "d3d12_logger.h"
#include "device.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// Buffer class implementation

Buffer::Buffer(Device& device) : device_(util::not_null{&device}) {}

Buffer::~Buffer() {}

bool Buffer::create() { return true; }

//@{ IBuffer

bool Buffer::updateVertexBuffer(std::span<const std::uint8_t> data, std::size_t offset) { return true; }

bool Buffer::updateConstantBuffer(std::span<const std::uint8_t> data, std::size_t offset) { return true; }

//@}
