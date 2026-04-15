#include "device.h"

#include "buffer.h"
#include "d3d12_logger.h"
#include "descriptor_set.h"
#include "pipeline.h"
#include "pipeline_layout.h"
#include "render_target.h"
#include "rendering_driver.h"
#include "sampler.h"
#include "shader_module.h"
#include "surface.h"
#include "swap_chain.h"
#include "texture.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// Device class implementation

Device::Device(RenderingDriver& instance, PhysicalDevice& physical_device)
    : instance_(util::not_null(&instance)), physical_device_(physical_device) {}

Device::~Device() {}

bool Device::create(const uxs::db::value& caps) {
    HRESULT result = D3D12CreateDevice(~physical_device_, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device_));
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create D3D12 device: {}", D3D12Result{result});
        return false;
    }

    D3D12MA::ALLOCATOR_DESC allocator_desc{
        .Flags = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS,
        .pDevice = device_.Get(),
        .pAdapter = ~physical_device_,
    };

    result = D3D12MA::CreateAllocator(&allocator_desc, &allocator_);
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create D3D12MA allocator: {}", result);
        return false;
    }

    if (!direct_queue_.create(*this, D3D12_COMMAND_LIST_TYPE_DIRECT)) { return false; }

    rtv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    dsv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    cbv_srv_uav_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    return true;
}

//@{ IDevice

bool Device::waitDevice() { return true; }

util::ref_ptr<IShaderModule> Device::createShaderModule(std::span<const std::uint32_t> source) {
    auto shader_module = util::make_new<ShaderModule>(*this);
    if (!shader_module->create(source)) { return nullptr; }
    return std::move(shader_module);
}

util::ref_ptr<IPipelineLayout> Device::createPipelineLayout(const uxs::db::value& config) {
    auto pipeline_layout = util::make_new<PipelineLayout>(*this);
    if (!pipeline_layout->create(config)) { return nullptr; }
    return std::move(pipeline_layout);
}

util::ref_ptr<IPipeline> Device::createPipeline(IRenderTarget& render_target, IPipelineLayout& pipeline_layout,
                                                std::span<IShaderModule* const> shader_modules,
                                                const uxs::db::value& config) {
    auto pipeline = util::make_new<Pipeline>(*this, static_cast<RenderTarget&>(render_target),
                                             static_cast<PipelineLayout&>(pipeline_layout));
    if (!pipeline->create(shader_modules, config)) { return nullptr; }
    return std::move(pipeline);
}

util::ref_ptr<IBuffer> Device::createBuffer(std::size_t size, BufferType type) {
    auto buffer = util::make_new<Buffer>(*this);
    if (!buffer->create()) { return nullptr; }
    return std::move(buffer);
}

util::ref_ptr<ITexture> Device::createTexture(const TextureOpts& opts) {
    auto texture = util::make_new<Texture>(*this);
    if (!texture->create(opts)) { return nullptr; }
    return std::move(texture);
}

util::ref_ptr<ISampler> Device::createSampler(const SamplerOpts& opts) {
    auto sampler = util::make_new<Sampler>(*this);
    if (!sampler->create(opts)) { return nullptr; }
    return std::move(sampler);
}

util::ref_ptr<IDescriptorSet> Device::createDescriptorSet(IPipelineLayout& pipeline_layout) {
    auto descriptor_set = util::make_new<DescriptorSet>(*this, static_cast<PipelineLayout&>(pipeline_layout));
    if (!descriptor_set->create()) { return nullptr; }
    return std::move(descriptor_set);
}

//@}
