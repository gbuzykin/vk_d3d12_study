#pragma once

#include "dev_queue.h"

#include "interfaces/i_rendering_driver.h"

namespace app3d::rel::d3d12 {

class RenderingDriver;
class PhysicalDevice;

class Device final : public util::ref_counter, public IDevice {
 public:
    Device(RenderingDriver& instance, PhysicalDevice& physical_device);
    ~Device() override;

    UINT getRtvDescriptorSize() const { return rtv_descriptor_size_; }
    UINT getDsvDescriptorSize() const { return dsv_descriptor_size_; }
    UINT getCbvSrvUavDescriptorSize() const { return cbv_srv_uav_descriptor_size_; }

    bool create(const uxs::db::value& caps);

    ID3D12Device* operator~() { return device_.Get(); }
    PhysicalDevice& getPhysicalDevice() { return physical_device_; }
    D3D12MA::Allocator* getAllocator() { return allocator_.Get(); }
    DevQueue& getDirectQueue() { return direct_queue_; }

    //@{ IDevice
    util::ref_counter& getRefCounter() override { return *this; }
    bool waitDevice() override;
    util::ref_ptr<IShaderModule> createShaderModule(std::span<const std::uint32_t> source) override;
    util::ref_ptr<IPipelineLayout> createPipelineLayout(const uxs::db::value& config) override;
    util::ref_ptr<IPipeline> createPipeline(IRenderTarget& render_target, IPipelineLayout& pipeline_layout,
                                            std::span<IShaderModule* const> shader_modules,
                                            const uxs::db::value& config) override;
    util::ref_ptr<IBuffer> createBuffer(std::size_t size, BufferType type) override;
    util::ref_ptr<ITexture> createTexture(const TextureOpts& opts) override;
    util::ref_ptr<ISampler> createSampler(const SamplerOpts& opts) override;
    util::ref_ptr<IDescriptorSet> createDescriptorSet(IPipelineLayout& pipeline_layout) override;
    //@}

 private:
    util::ref_ptr<RenderingDriver> instance_;
    PhysicalDevice& physical_device_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> allocator_;
    DevQueue direct_queue_;
    UINT rtv_descriptor_size_ = 0;
    UINT dsv_descriptor_size_ = 0;
    UINT cbv_srv_uav_descriptor_size_ = 0;
};

}  // namespace app3d::rel::d3d12
