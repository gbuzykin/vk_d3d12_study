#pragma once

#include "d3d12_api.h"

#include "interfaces/i_rendering_driver.h"

#include <uxs/dynarray.h>

#include <array>

namespace app3d::rel::d3d12 {

class Device;
class DescriptorSet;

class PipelineLayout : public util::ref_counter, public IPipelineLayout {
 public:
    explicit PipelineLayout(Device& device);
    ~PipelineLayout();

    enum class HeapType { CBV_SRV_UAV = 0, SAMPLER, TOTAL_COUNT };

    template<typename Ty>
    using PerBindingType = std::array<Ty, unsigned(BindingType::TOTAL_COUNT)>;

    template<typename Ty>
    using PerHeapType = std::array<Ty, unsigned(HeapType::TOTAL_COUNT)>;

    static constexpr std::array TBL_D3D12_HEAP_TYPES{
        // HeapType::
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,  // CBV_SRV_UAV
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,      // SAMPLER
    };

    struct Binding {
        std::uint32_t binding_info;
        std::uint32_t handle_offset;
    };

    struct DescriptorSetLayout {
        std::uint32_t param_count;
        std::uint32_t first_param_index;
        PerBindingType<std::uint32_t> slot_binding_offsets;
        PerHeapType<std::uint32_t> desc_counts;
    };

    struct DescriptorSetHandles {
        const DescriptorSetLayout* set_layout;
        uxs::inline_dynarray<D3D12_GPU_VIRTUAL_ADDRESS> base_resource_addresses;
        PerHeapType<D3D12_GPU_DESCRIPTOR_HANDLE> base_gpu_handles;
        PerHeapType<D3D12_CPU_DESCRIPTOR_HANDLE> base_cpu_handles;
    };

    const Binding* getParamBinding(const DescriptorSetLayout& set_layout) const {
        return &param_bindings_[set_layout.first_param_index];
    }

    // const SlotBindings* getSlotBindings(const DescriptorSetLayout& set_layout) const {
    //     return &slot_bindings_[set_layout.slot_table_start];
    // }

    bool create(const uxs::db::value& config);
    bool obtainDescriptorSet(std::uint32_t set_layout_index, DescriptorSetHandles& handles);
    void bindRootSignature(ID3D12GraphicsCommandList* command_list);
    void bindRootDescriptorTables(ID3D12GraphicsCommandList* command_list, std::uint32_t set_index,
                                  DescriptorSet& desc_set, std::span<const std::uint32_t> offsets);

    ID3D12RootSignature* getD3D12RootSignature() { return root_signature_.get(); }

    //@{ IPipelineLayout
    util::ref_counter& getRefCounter() override { return *this; }
    util::ref_ptr<IDescriptorSet> createDescriptorSet(std::uint32_t set_layout_index) override;
    void resetDescriptorAllocator() override;
    //@}

 private:
    util::ref_ptr<Device> device_;
    util::ref_ptr<ID3D12RootSignature> root_signature_;

    struct DescriptorHeap {
        std::uint32_t total_count = 0;
        std::uint32_t used_count = 0;
        util::ref_ptr<ID3D12DescriptorHeap> heap;
    };

    PerHeapType<DescriptorHeap> heaps_{};
    PerHeapType<std::uint32_t> d3d12_desc_sizes_{};
    uxs::inline_dynarray<DescriptorSetLayout> set_layouts_;
    uxs::inline_dynarray<Binding, 64> param_bindings_;  // binding_info: 0:3 - binding type,
                                                        //               4:31 - dynamic resource index + 1
    uxs::inline_dynarray<Binding, 64> slot_bindings_;   // binding_info: dynamic resource index + 1

    bool createDescriptorPool();
};

}  // namespace app3d::rel::d3d12
