#include "descriptor_set.h"

#include "buffer.h"
#include "device.h"
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

//@{ IDescriptorSet

void DescriptorSet::updateCombinedTextureSamplerDescriptor(ITexture& texture, ISampler& sampler, std::uint32_t slot) {
    const auto& srv_desc = static_cast<Texture&>(texture).getShaderResourceViewDesc();
    const auto& sampler_desc = static_cast<Sampler&>(sampler).getSamplerDesc();
    const auto* slot_bindings = getLayout().getSlotBindings(*handles_.set_layout);
    device_->getD3D12Device()->CreateShaderResourceView(
        static_cast<Texture&>(texture).getResource(), &srv_desc,
        handles_.base_cpu_handles[unsigned(HeapType::CBV_SRV_UAV)] +
            slot_bindings[slot][unsigned(BindingType::SHADER_RESOURCE)].handle_offset);
    device_->getD3D12Device()->CreateSampler(&sampler_desc,
                                             handles_.base_cpu_handles[unsigned(HeapType::SAMPLER)] +
                                                 slot_bindings[slot][unsigned(BindingType::SAMPLER)].handle_offset);
}

void DescriptorSet::updateConstantBufferDescriptor(IBuffer& buffer, std::uint64_t offset, std::uint64_t size,
                                                   std::uint32_t slot) {
    const auto& cbv_desc = static_cast<Buffer&>(buffer).getConstantBufferViewDesc(offset);
    const auto* slot_bindings = getLayout().getSlotBindings(*handles_.set_layout);
    device_->getD3D12Device()->CreateConstantBufferView(
        &cbv_desc, handles_.base_cpu_handles[unsigned(HeapType::CBV_SRV_UAV)] +
                       slot_bindings[slot][unsigned(BindingType::CONSTANT_BUFFER)].handle_offset);
    //    device_->getD3D12Device()->SetGraphicsRootConstantBufferView();
}

//@}
