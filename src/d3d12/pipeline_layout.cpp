#include "pipeline_layout.h"

#include "d3d12_logger.h"
#include "descriptor_set.h"
#include "device.h"
#include "pipeline.h"
#include "render_target.h"
#include "shader_module.h"
#include "tables.h"

#include "rel/tables.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// PipelineLayout class implementation

PipelineLayout::PipelineLayout(Device& device) : device_(util::not_null(&device)) {}

PipelineLayout::~PipelineLayout() {}

bool PipelineLayout::create(const uxs::db::value& config) {
    d3d12_desc_sizes_[unsigned(HeapType::CBV_SRV_UAV)] = device_->getCbvSrvUavDescriptorSize();
    d3d12_desc_sizes_[unsigned(HeapType::SAMPLER)] = device_->getSamplerDescriptorSize();

    struct DescriptorRange {
        BindingType binding_type;
        std::uint32_t slot;
        std::uint32_t count;
    };

    struct RootParameter {
        ShaderStage visibility;
        HeapType heap_type;
        std::uint32_t dynamic_resource_index;  //  dynamic resource index + 1
        std::uint32_t first_range;
        std::uint32_t range_count;
    };

    struct SlotBindingRange {
        Binding binding;
        std::uint32_t slot;
        std::uint32_t count;
    };

    std::uint32_t def_space_index = 0;
    std::uint32_t first_param_index = 0;
    uxs::inline_dynarray<RootParameter, 32> params;
    uxs::inline_dynarray<DescriptorRange, 32> ranges;
    PerBindingType<uxs::inline_dynarray<SlotBindingRange, 32>> binding_ranges;

    uxs::inline_dynarray<D3D12_DESCRIPTOR_RANGE, 32> d3d12_ranges;

    const auto& descriptor_set_layouts = config.value("descriptor_set_layouts");

    for (const auto& layout : descriptor_set_layouts.as_array()) {
        const auto& list = layout.value("descriptor_list");
        const std::uint32_t space_index = layout.value_or<std::uint32_t>("space_index", def_space_index);
        const std::uint32_t max_sets = layout.value_or<std::uint32_t>("max_sets", 8);
        def_space_index = space_index + 1;

        params.clear();
        ranges.clear();

        PerBindingType<std::uint32_t> next_slots{};

        for (const auto& desc : list.as_array()) {
            const auto type = parseDescriptorType(desc.value("type").as_string_view());
            const auto binding_type = TBL_DESC_BINDING_TYPE[unsigned(type)];
            const std::uint32_t slot = desc.value_or<std::uint32_t>("slot", next_slots[unsigned(binding_type)]);
            const std::uint32_t desc_count = desc.value_or<std::uint32_t>("count", 1);
            next_slots[unsigned(binding_type)] = slot + desc_count;

            const auto visibility = parseShaderStage(desc.value_or<const char*>("shader_visibility", "ALL"));

            const auto append_range = [&params, &ranges, &heaps = heaps_, desc_count, visibility, max_sets](
                                          auto binding_type, auto slot) {
                const auto heap_type = binding_type == BindingType::SAMPLER ? HeapType::SAMPLER : HeapType::CBV_SRV_UAV;

                auto param_it = std::ranges::find_if(params, [visibility, heap_type](const auto& param) {
                    return param.visibility == visibility && param.heap_type == heap_type;
                });

                ranges.emplace_back(DescriptorRange{.binding_type = binding_type, .slot = slot, .count = desc_count});

                if (param_it != params.end()) {
                    if (param_it != params.end() - 1) {
                        const auto t = ranges.back();
                        std::copy_backward(ranges.begin() + param_it->first_range + param_it->range_count,
                                           ranges.end() - 1, ranges.end());
                        ranges[param_it->first_range + param_it->range_count] = t;
                        std::for_each(param_it + 1, params.end(), [](auto& param) { ++param.first_range; });
                    }
                    ++param_it->range_count;
                } else {
                    params.emplace_back(RootParameter{
                        .visibility = visibility,
                        .heap_type = heap_type,
                        .first_range = std::uint32_t(ranges.size() - 1),
                        .range_count = 1,
                    });
                }

                heaps[unsigned(heap_type)].total_count += max_sets * desc_count;
            };

            append_range(binding_type, slot);
            if (type == DescriptorType::COMBINED_TEXTURE_SAMPLER) {
                const std::uint32_t sampler_slot = desc.value_or<std::uint32_t>(
                    "sampler_slot", next_slots[unsigned(BindingType::SAMPLER)]);
                append_range(BindingType::SAMPLER, sampler_slot);
            }
        }

        auto& set_layout = set_layouts_.emplace_back(DescriptorSetLayout{
            .param_count = std::uint32_t(params.size()) - first_param_index,
            .first_param_index = first_param_index,
        });

        for (auto& range : binding_ranges) { range.clear(); }

        for (const auto& param : params) {
            const std::uint32_t d3d12_desc_size = d3d12_desc_sizes_[unsigned(param.heap_type)];
            std::uint32_t& total_desc_count = set_layout.desc_counts[unsigned(param.heap_type)];
            std::uint32_t handle_offset = total_desc_count * d3d12_desc_size;

            param_bindings_.emplace_back(
                ParamBinding{.handle_offset = (handle_offset << 4) & std::uint32_t(param.heap_type)});

            for (const auto& range : std::span{ranges}.subspan(param.first_range, param.range_count)) {
                if (set_layout.slot_table_start + range.slot >= slot_bindings_.size()) {
                    slot_bindings_.resize(set_layout.slot_table_start + range.slot + 1);
                }
                for (std::uint32_t n = 0; n < range.count; ++n, handle_offset += d3d12_desc_size) {
                    slot_bindings_[set_layout.slot_table_start + range.slot][unsigned(range.binding_type)] = SlotBinding{
                        .is_dynamic = param.is_dynamic, .handle_offset = handle_offset};
                }

                total_desc_count += range.count;
            }
        }

        for (const auto& range : ranges) {
            d3d12_ranges.emplace_back(D3D12_DESCRIPTOR_RANGE{
                .RangeType = TBL_D3D12_DESC_TYPES[unsigned(range.binding_type)],
                .NumDescriptors = UINT(range.count),
                .BaseShaderRegister = UINT(range.slot),
                .RegisterSpace = UINT(space_index),
                .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
            });
        }

        first_param_index += std::uint32_t(params.size());
    }

    const D3D12_DESCRIPTOR_RANGE* d3d12_range = d3d12_ranges.data();
    uxs::inline_dynarray<D3D12_ROOT_PARAMETER> d3d12_root_params;
    for (const auto& param : params) {
        d3d12_root_params.emplace_back(D3D12_ROOT_PARAMETER{
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable =
                {
                    .NumDescriptorRanges = UINT(param.range_count),
                    .pDescriptorRanges = d3d12_range,
                },
            .ShaderVisibility = TBL_D3D12_VISIBILITY[unsigned(param.visibility)],
        });

        d3d12_range += param.range_count;
    }

    util::ref_ptr<ID3DBlob> serialized_root_sig;
    util::ref_ptr<ID3DBlob> errors;
    HRESULT result = ::D3D12SerializeRootSignature(
        constAddressOf(D3D12_ROOT_SIGNATURE_DESC{
            .NumParameters = UINT(d3d12_root_params.size()),
            .pParameters = d3d12_root_params.data(),
            .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
        }),
        D3D_ROOT_SIGNATURE_VERSION_1, serialized_root_sig.reset_and_get_address(), errors.reset_and_get_address());
    const auto error_text = errors ? std::string_view(static_cast<const char*>(errors->GetBufferPointer()),
                                                      errors->GetBufferSize()) :
                                     std::string_view{};
    if (result != S_OK) {
        logError(LOG_D3D12 "{}", error_text);
        return false;
    }

    if (errors) { logWarning(LOG_D3D12 "{}", error_text); }

    result = device_->getD3D12Device()->CreateRootSignature(0, serialized_root_sig->GetBufferPointer(),
                                                            serialized_root_sig->GetBufferSize(),
                                                            IID_PPV_ARGS(root_signature_.reset_and_get_address()));
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create root signature: {}", D3D12Result(result));
        return false;
    }

    return createDescriptorPool();
}

bool PipelineLayout::obtainDescriptorSet(std::uint32_t set_layout_index, DescriptorSetHandles& handles) {
    const auto& set_layout = set_layouts_[set_layout_index];

    for (std::uint32_t n = 0; n < std::uint32_t(HeapType::TOTAL_COUNT); ++n) {
        auto& heap = heaps_[n];
        if (heap.total_count - heap.used_count < set_layout.desc_counts[n]) {
            logError(LOG_D3D12 "out of descriptor heap");
            return false;
        }

        handles.base_cpu_handles[n] = heap.heap->GetCPUDescriptorHandleForHeapStart() +
                                      heap.used_count * d3d12_desc_sizes_[n];
        handles.base_gpu_handles[n] = heap.heap->GetGPUDescriptorHandleForHeapStart() +
                                      heap.used_count * d3d12_desc_sizes_[n];

        heap.used_count += set_layout.desc_counts[n];
    }

    handles.set_layout = &set_layouts_[set_layout_index];
    return true;
}

namespace {
template<typename Ty, std::size_t N, std::size_t... Indices>
auto makeHeapsArray(const std::array<Ty, N>& heaps, std::index_sequence<Indices...>) {
    return std::array{heaps[Indices].heap.get()...};
}
}  // namespace

void PipelineLayout::bindRootSignature(ID3D12GraphicsCommandList* command_list) {
    const auto heaps = makeHeapsArray(heaps_, std::make_index_sequence<std::size_t(HeapType::TOTAL_COUNT)>{});
    command_list->SetDescriptorHeaps(UINT(heaps.size()), heaps.data());
    command_list->SetGraphicsRootSignature(root_signature_.get());
}

void PipelineLayout::bindRootDescriptorTables(ID3D12GraphicsCommandList* command_list, std::uint32_t set_index,
                                              DescriptorSet& desc_set, std::span<const std::uint32_t> offsets) {
    const DescriptorSetHandles& handles = desc_set.getDescriptorHandles();
    const auto* param_binding = desc_set.getLayout().getParamBinding(*handles.set_layout);
    const auto& set_layout = set_layouts_[set_index];
    for (std::uint32_t n = 0; n < set_layout.param_count; ++n, ++param_binding) {
        command_list->SetGraphicsRootDescriptorTable(
            set_layout.first_param_index + n, handles.base_gpu_handles[*param_binding & 15] + param_binding->handle_offset);
    }
}

//@{ IPipelineLayout

util::ref_ptr<IDescriptorSet> PipelineLayout::createDescriptorSet(std::uint32_t set_layout_index) {
    auto descriptor_set = util::make_new<DescriptorSet>(*device_, *this);
    if (!descriptor_set->create(set_layout_index)) { return nullptr; }
    return std::move(descriptor_set);
}

void PipelineLayout::resetDescriptorAllocator() {
    for (auto& heap : heaps_) { heap.used_count = 0; }
}

//@}

bool PipelineLayout::createDescriptorPool() {
    for (std::uint32_t n = 0; n < std::uint32_t(HeapType::TOTAL_COUNT); ++n) {
        const D3D12_DESCRIPTOR_HEAP_DESC heap_desc{
            .Type = TBL_D3D12_HEAP_TYPES[n],
            .NumDescriptors = UINT(heaps_[n].total_count),
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            .NodeMask = 0,
        };

        HRESULT result = device_->getD3D12Device()->CreateDescriptorHeap(
            &heap_desc, IID_PPV_ARGS(heaps_[n].heap.reset_and_get_address()));
        if (result != S_OK) {
            logError(LOG_D3D12 "couldn't create CBV, SRV or UAV heap: {}", D3D12Result(result));
            return false;
        }
    }

    return true;
}
