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

    enum class BindingType { CONSTANT_BUFFER = 0, SHADER_RESOURCE, UNORDERED, SAMPLER, TOTAL_COUNT };

    static constexpr std::array binding_types{
        BindingType::SHADER_RESOURCE,
        BindingType::CONSTANT_BUFFER,
        BindingType::CONSTANT_BUFFER,
    };

    static constexpr std::array vulkan_desc_types{
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    };

    struct Binding {
        DescriptorType type;
        std::uint32_t binding;
        std::uint32_t array_element;
    };

    using SlotBindings = std::array<Binding, unsigned(BindingType::TOTAL_COUNT)>;

    struct DescriptorSetHandle {
        const SlotBindings* bindings;
        VkDescriptorSet handle;
    };

    bool create(const uxs::db::value& config);
    bool obtainDescriptorSet(std::uint32_t set_layout_index, DescriptorSetHandle& handle);

    VkPipelineLayout operator~() { return pipeline_layout_; }

    //@{ IPipelineLayout
    //@}

    //@{ IPipelineLayout
    util::ref_counter& getRefCounter() override { return *this; }
    util::ref_ptr<IDescriptorSet> createDescriptorSet(std::uint32_t set_layout_index) override;
    //@}

 private:
    util::ref_ptr<Device> device_;
    VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};

    uxs::inline_dynarray<VkDescriptorSetLayout> set_layouts_;
    uxs::inline_dynarray<std::uint32_t> binding_offsets_;
    uxs::inline_dynarray<SlotBindings, 32> bindings_;

    void setBinding(BindingType binding_type, std::uint32_t slot, DescriptorType type, std::uint32_t binding,
                    std::uint32_t count) {
        if (slot + count > bindings_.size()) { bindings_.resize(slot + count); }
        for (std::uint32_t n = 0; n < count; ++n) {
            bindings_[slot + n][unsigned(binding_type)] = Binding{type, binding, n};
        }
    }
};

}  // namespace app3d::rel::vulkan
