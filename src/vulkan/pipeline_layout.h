#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

#include <uxs/dynarray.h>

#include <array>

namespace app3d::rel::vulkan {

class Device;

class PipelineLayout : public util::ref_counter, public IPipelineLayout {
 public:
    explicit PipelineLayout(Device& device);
    ~PipelineLayout();

    template<typename Ty>
    using PerBindingType = std::array<Ty, unsigned(BindingType::TOTAL_COUNT)>;

    struct Binding {
        DescriptorType desc_type;
        std::uint32_t binding;
        std::uint32_t array_element;
    };

    struct DescriptorSetHandle {
        const PerBindingType<std::uint32_t>* binding_offsets;
        VkDescriptorSet handle;
    };

    const Binding& getBinding(std::uint32_t binding_index) const { return bindings_[binding_index]; }

    bool create(const uxs::db::value& config);
    bool obtainDescriptorSet(std::uint32_t set_layout_index, DescriptorSetHandle& handle);

    VkPipelineLayout getHandle() { return pipeline_layout_; }

    //@{ IPipelineLayout
    util::ref_counter& getRefCounter() override { return *this; }
    util::ref_ptr<IDescriptorSet> createDescriptorSet(std::uint32_t set_layout_index) override;
    void resetDescriptorAllocator() override;
    //@}

 private:
    util::ref_ptr<Device> device_;
    VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
    VkDescriptorPool desc_pool_{VK_NULL_HANDLE};

    uxs::inline_dynarray<VkDescriptorSetLayout> set_layouts_;
    uxs::inline_dynarray<PerBindingType<std::uint32_t>> binding_offsets_;
    uxs::inline_dynarray<Binding, 64> bindings_;

    bool createDescriptorPool(std::uint32_t total_max_sets, std::span<const VkDescriptorPoolSize> desc_counts);
};

}  // namespace app3d::rel::vulkan
