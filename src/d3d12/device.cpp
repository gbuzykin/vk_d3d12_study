#include "device.h"

#include "buffer.h"
#include "d3d12_logger.h"
#include "pipeline.h"
#include "pipeline_layout.h"
#include "render_target.h"
#include "rendering_driver.h"
#include "sampler.h"
#include "shader_module.h"
#include "surface.h"
#include "swap_chain.h"
#include "tables.h"
#include "texture.h"
#include "wrappers.h"

#include "rel/tables.h"

#include <cstring>

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// Device class implementation

Device::Device(RenderingDriver& instance, PhysicalDevice& physical_device)
    : instance_(util::not_null(&instance)), physical_device_(physical_device) {}

Device::~Device() {
    for (auto& kit : transfer_kits_) { kit.fence.wait(); }
}

bool Device::create(const uxs::db::value& caps) {
    HRESULT result = ::D3D12CreateDevice(physical_device_.getAdapter(), D3D_FEATURE_LEVEL_12_0,
                                         IID_PPV_ARGS(device_.reset_and_get_address()));
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create D3D12 device: {}", D3D12Result{result});
        return false;
    }

    const D3D12MA::ALLOCATOR_DESC allocator_desc{
        .Flags = D3D12MA::ALLOCATOR_FLAGS(D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS),
        .pDevice = device_.get(),
        .pAdapter = physical_device_.getAdapter(),
    };

    result = D3D12MA::CreateAllocator(&allocator_desc, allocator_.reset_and_get_address());
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create D3D12MA allocator: {}", D3D12Result(result));
        return false;
    }

    if (!direct_queue_.create(*this, D3D12_COMMAND_LIST_TYPE_DIRECT, 0)) { return false; }
    if (!transfer_queue_.create(*this, D3D12_COMMAND_LIST_TYPE_DIRECT, TRANSFER_KIT_COUNT)) { return false; }

    rtv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    dsv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    cbv_srv_uav_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    sampler_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    transfer_kits_.resize(TRANSFER_KIT_COUNT);
    for (std::uint32_t n = 0; n < TRANSFER_KIT_COUNT; ++n) {
        if (!transfer_kits_[n].fence.create(*this)) { return false; }
        if (!(transfer_kits_[n].command_list = transfer_queue_.createGraphicsCommandList(n))) { return false; }
    }

    return true;
}

bool Device::updateBuffer(std::span<const std::uint8_t> data, ID3D12Resource* resource, std::uint64_t offset,
                          D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after) {
    auto& kit = transfer_kits_[current_transfer_kit_];

    if (!kit.fence.wait()) { return false; }

    if (kit.staging_buffer.size < data.size()) {
        kit.staging_buffer.unmap();
        if (!createStagingBuffer(data.size(), kit.staging_buffer) || !kit.staging_buffer.map()) { return false; }
    }

    std::memcpy(kit.staging_buffer.ptr, data.data(), data.size());

    if (!transfer_queue_.resetAllocator(current_transfer_kit_)) { return false; }

    if (!transfer_queue_.resetCommandList(current_transfer_kit_, kit.command_list.get(), nullptr)) { return false; }

    kit.command_list->ResourceBarrier(1, constAddressOf(Wrapper<D3D12_RESOURCE_BARRIER>::transition({
                                             .resource = resource,
                                             .state_before = state_before,
                                             .state_after = D3D12_RESOURCE_STATE_COPY_DEST,
                                         })));

    kit.command_list->CopyBufferRegion(resource, offset, kit.staging_buffer.allocation->GetResource(), 0, data.size());

    kit.command_list->ResourceBarrier(1, constAddressOf(Wrapper<D3D12_RESOURCE_BARRIER>::transition({
                                             .resource = resource,
                                             .state_before = D3D12_RESOURCE_STATE_COPY_DEST,
                                             .state_after = state_after,
                                         })));

    if (!DevQueue::closeCommandList(kit.command_list.get())) { return false; }

    transfer_queue_.executeCommandLists(std::array{static_cast<ID3D12CommandList*>(kit.command_list.get())});

    if (!kit.fence.queueSignal(transfer_queue_)) { return false; }

    if (++current_transfer_kit_ == TRANSFER_KIT_COUNT) { current_transfer_kit_ = 0; }
    return true;
}

namespace {
inline void memcpySubresource(const D3D12_MEMCPY_DEST& dest, const D3D12_SUBRESOURCE_DATA& src,
                              std::size_t row_size_in_bytes, std::uint32_t num_rows, std::uint32_t num_slices) {
    auto* dest_slice = static_cast<std::uint8_t*>(dest.pData);
    const auto* src_slice = static_cast<const std::uint8_t*>(src.pData);
    if (dest.RowPitch == src.RowPitch && row_size_in_bytes == dest.RowPitch) {
        for (std::uint32_t z = 0; z < num_slices; ++z, dest_slice += dest.SlicePitch, src_slice += src.SlicePitch) {
            std::memcpy(dest_slice, src_slice, row_size_in_bytes * num_rows);
        }
    } else {
        for (std::uint32_t z = 0; z < num_slices; ++z, dest_slice += dest.SlicePitch, src_slice += src.SlicePitch) {
            auto* dest_row = dest_slice;
            const auto* src_row = src_slice;
            for (std::uint32_t y = 0; y < num_rows; ++y, dest_row += dest.RowPitch, src_row += src.RowPitch) {
                std::memcpy(dest_row, src_row, row_size_in_bytes);
            }
        }
    }
}
}  // namespace

bool Device::updateTexture(const std::uint8_t* data, ID3D12Resource* resource, Format format,
                           const D3D12_RESOURCE_DESC& resource_desc, std::uint32_t first_subresource,
                           std::span<const UpdateTextureDesc> update_subresource_descs,
                           D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after) {
    auto& kit = transfer_kits_[current_transfer_kit_];

    constexpr std::size_t alloc_size_per_subresource = sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) + sizeof(UINT64) +
                                                       sizeof(UINT);

    const std::uint32_t num_subresources = std::uint32_t(update_subresource_descs.size());

    uxs::inline_dynarray<UINT64, (alloc_size_per_subresource * 16 + sizeof(UINT64) - 1) / sizeof(UINT64)> mem;
    mem.resize((alloc_size_per_subresource * num_subresources + sizeof(UINT64) - 1) / sizeof(UINT64));

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layouts = reinterpret_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(mem.data());
    UINT64* row_size_in_bytes = reinterpret_cast<UINT64*>(layouts + num_subresources);
    UINT* num_rows = reinterpret_cast<UINT*>(row_size_in_bytes + num_subresources);
    UINT64 required_size = 0;

    device_->GetCopyableFootprints(&resource_desc, first_subresource, num_subresources, 0, layouts, num_rows,
                                   row_size_in_bytes, &required_size);

    if (!kit.fence.wait()) { return false; }

    if (kit.staging_buffer.size < required_size) {
        kit.staging_buffer.unmap();
        if (!createStagingBuffer(required_size, kit.staging_buffer) || !kit.staging_buffer.map()) { return false; }
    }

    const std::uint32_t bytes_per_pixel = TBL_FORMAT_SIZE[unsigned(format)];

    auto size_of_subresource = [bytes_per_pixel](const auto& desc) {
        return (desc.buffer_row_size ?
                    std::size_t(desc.buffer_row_size) * desc.buffer_row_count :
                    std::size_t(std::size_t(desc.image_extent.width * bytes_per_pixel)) * desc.image_extent.height) *
               desc.image_extent.depth;
    };

    std::size_t buf_offset = 0;

    for (std::uint32_t n = 0; n < num_subresources; ++n) {
        const auto& desc = update_subresource_descs[n];

        const D3D12_SUBRESOURCE_DATA src_data{
            .pData = data + (desc.buffer_offset ? desc.buffer_offset : buf_offset),
            .RowPitch = desc.buffer_row_size ? LONG_PTR(desc.buffer_row_size) :
                                               LONG_PTR(desc.image_extent.width) * bytes_per_pixel,
            .SlicePitch = desc.buffer_row_size ?
                              LONG_PTR(desc.buffer_row_size) * desc.buffer_row_count :
                              LONG_PTR(desc.image_extent.width * bytes_per_pixel) * desc.image_extent.height,
        };

        const D3D12_MEMCPY_DEST dest_data{
            .pData = static_cast<std::uint8_t*>(kit.staging_buffer.ptr) + layouts[n].Offset,
            .RowPitch = SIZE_T(layouts[n].Footprint.RowPitch),
            .SlicePitch = SIZE_T(layouts[n].Footprint.RowPitch) * num_rows[n],
        };

        memcpySubresource(dest_data, src_data,
                          desc.image_extent.width < layouts[n].Footprint.Width ?
                              desc.image_extent.width * bytes_per_pixel :
                              row_size_in_bytes[n],
                          num_rows[n], desc.image_extent.depth);
        buf_offset += size_of_subresource(desc);
    }

    if (!transfer_queue_.resetAllocator(current_transfer_kit_)) { return false; }

    if (!transfer_queue_.resetCommandList(current_transfer_kit_, kit.command_list.get(), nullptr)) { return false; }

    kit.command_list->ResourceBarrier(1, constAddressOf(Wrapper<D3D12_RESOURCE_BARRIER>::transition({
                                             .resource = resource,
                                             .state_before = state_before,
                                             .state_after = D3D12_RESOURCE_STATE_COPY_DEST,
                                         })));

    for (std::uint32_t n = 0; n < num_subresources; ++n) {
        const auto& desc = update_subresource_descs[n];

        const D3D12_TEXTURE_COPY_LOCATION dst{
            .pResource = resource,
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            .SubresourceIndex = UINT(first_subresource + n),
        };

        const D3D12_TEXTURE_COPY_LOCATION src{
            .pResource = kit.staging_buffer.allocation->GetResource(),
            .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
            .PlacedFootprint = layouts[n],
        };

        kit.command_list->CopyTextureRegion(&dst, UINT(desc.image_offset.x), UINT(desc.image_offset.y),
                                            UINT(desc.image_offset.z), &src,
                                            constAddressOf(D3D12_BOX{
                                                .right = UINT(desc.image_extent.width),
                                                .bottom = UINT(desc.image_extent.height),
                                                .back = UINT(desc.image_extent.depth),
                                            }));
    }

    kit.command_list->ResourceBarrier(1, constAddressOf(Wrapper<D3D12_RESOURCE_BARRIER>::transition({
                                             .resource = resource,
                                             .state_before = D3D12_RESOURCE_STATE_COPY_DEST,
                                             .state_after = state_after,
                                         })));

    if (!DevQueue::closeCommandList(kit.command_list.get())) { return false; }

    transfer_queue_.executeCommandLists(std::array{static_cast<ID3D12CommandList*>(kit.command_list.get())});

    if (!kit.fence.queueSignal(transfer_queue_)) { return false; }

    if (++current_transfer_kit_ == TRANSFER_KIT_COUNT) { current_transfer_kit_ = 0; }
    return true;
}

//@{ IDevice

bool Device::waitDevice() {
    if (!direct_queue_.wait()) { return false; }
    if (!transfer_queue_.wait()) { return false; }
    return true;
}

util::ref_ptr<ISwapChain> Device::createSwapChain(ISurface& surface, const uxs::db::value& opts) {
    auto swap_chain = util::make_new<SwapChain>(*this, static_cast<Surface&>(surface));
    if (!swap_chain->create(opts)) { return nullptr; }
    return std::move(swap_chain);
}

util::ref_ptr<IShaderModule> Device::createShaderModule(DataBlob bytecode) {
    auto shader_module = util::make_new<ShaderModule>(*this);
    if (!shader_module->create(std::move(bytecode))) { return nullptr; }
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

util::ref_ptr<IBuffer> Device::createBuffer(BufferType type, std::uint64_t size) {
    auto buffer = util::make_new<Buffer>(*this);
    if (!buffer->create(type, size)) { return nullptr; }
    return std::move(buffer);
}

util::ref_ptr<ITexture> Device::createTexture(const TextureDesc& desc) {
    auto texture = util::make_new<Texture>(*this);
    if (!texture->create(desc)) { return nullptr; }
    return std::move(texture);
}

util::ref_ptr<ISampler> Device::createSampler(const SamplerDesc& desc) {
    auto sampler = util::make_new<Sampler>(*this);
    if (!sampler->create(desc)) { return nullptr; }
    return std::move(sampler);
}

//@}

bool Device::StagingBuffer::map() {
    HRESULT result = allocation->GetResource()->Map(0, nullptr, &ptr);
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't map staging buffer: {}", D3D12Result(result));
        return false;
    }
    return true;
}

void Device::StagingBuffer::unmap() {
    if (allocation) { allocation->GetResource()->Unmap(0, nullptr); }
}

bool Device::createStagingBuffer(UINT64 size, StagingBuffer& buffer) {
    const D3D12MA::ALLOCATION_DESC allocation_desc = {
        .HeapType = D3D12_HEAP_TYPE_UPLOAD,
    };

    const D3D12_RESOURCE_DESC staging_buffer_desc{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = size,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE,
    };

    HRESULT result = allocator_->CreateResource(&allocation_desc, &staging_buffer_desc,
                                                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                buffer.allocation.reset_and_get_address(), IID_NULL, nullptr);
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create buffer resource: {}", D3D12Result(result));
        return false;
    }

    buffer.size = size;
    return true;
}
