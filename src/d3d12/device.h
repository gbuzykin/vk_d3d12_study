#pragma once

#include "dev_queue.h"

#include "interfaces/i_rendering_driver.h"

#include <uxs/dynarray.h>

namespace app3d::rel::d3d12 {

class RenderingDriver;
class PhysicalDevice;

class Device final : public util::ref_counter, public IDevice {
 public:
    Device(RenderingDriver& instance, PhysicalDevice& physical_device);
    ~Device() override;

    std::uint32_t getRtvDescriptorSize() const { return rtv_descriptor_size_; }
    std::uint32_t getDsvDescriptorSize() const { return dsv_descriptor_size_; }
    std::uint32_t getCbvSrvUavDescriptorSize() const { return cbv_srv_uav_descriptor_size_; }
    std::uint32_t getSamplerDescriptorSize() const { return sampler_descriptor_size_; }

    bool create(const uxs::db::value& caps);

    bool updateBuffer(std::span<const std::uint8_t> data, ID3D12Resource* resource, std::uint64_t offset,
                      D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);
    bool updateTexture(const std::uint8_t* data, ID3D12Resource* resource, Format format,
                       const D3D12_RESOURCE_DESC& resource_desc, std::uint32_t first_subresource,
                       std::span<const UpdateTextureDesc> update_subresource_descs, D3D12_RESOURCE_STATES state_before,
                       D3D12_RESOURCE_STATES state_after);

    ID3D12Device* getD3D12Device() { return device_.get(); }
    PhysicalDevice& getPhysicalDevice() { return physical_device_; }
    D3D12MA::Allocator* getAllocator() { return allocator_.get(); }
    DevQueue& getDirectQueue() { return direct_queue_; }

    //@{ IDevice
    util::ref_counter& getRefCounter() override { return *this; }
    bool waitDevice() override;
    util::ref_ptr<ISwapChain> createSwapChain(ISurface& surface, const uxs::db::value& opts) override;
    util::ref_ptr<IShaderModule> createShaderModule(DataBlob bytecode) override;
    util::ref_ptr<IPipelineLayout> createPipelineLayout(const uxs::db::value& config) override;
    util::ref_ptr<IPipeline> createPipeline(IRenderTarget& render_target, IPipelineLayout& pipeline_layout,
                                            std::span<IShaderModule* const> shader_modules,
                                            const uxs::db::value& config) override;
    util::ref_ptr<IBuffer> createBuffer(BufferType type, std::uint64_t size) override;
    util::ref_ptr<ITexture> createTexture(const TextureDesc& desc) override;
    util::ref_ptr<ISampler> createSampler(const SamplerDesc& desc) override;
    //@}

 private:
    util::ref_ptr<RenderingDriver> instance_;
    PhysicalDevice& physical_device_;
    util::ref_ptr<ID3D12Device> device_;
    util::ref_ptr<D3D12MA::Allocator> allocator_;
    DevQueue direct_queue_;
    DevQueue transfer_queue_;
    std::uint32_t rtv_descriptor_size_ = 0;
    std::uint32_t dsv_descriptor_size_ = 0;
    std::uint32_t cbv_srv_uav_descriptor_size_ = 0;
    std::uint32_t sampler_descriptor_size_ = 0;

    struct StagingBuffer {
        ~StagingBuffer() { unmap(); }
        bool map();
        void unmap();
        util::ref_ptr<D3D12MA::Allocation> allocation;
        UINT64 size = 0;
        void* ptr = nullptr;
    };

    struct TransferKit {
        Fence fence;
        StagingBuffer staging_buffer;
        util::ref_ptr<ID3D12GraphicsCommandList> command_list;
    };

    static constexpr std::uint32_t TRANSFER_KIT_COUNT = 1;
    std::uint32_t current_transfer_kit_ = 0;
    uxs::inline_dynarray<TransferKit, TRANSFER_KIT_COUNT> transfer_kits_;

    bool createStagingBuffer(UINT64 size, StagingBuffer& buffer);
};

}  // namespace app3d::rel::d3d12
