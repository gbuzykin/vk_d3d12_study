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

    enum class BindingType {
        TEXTURE_SAMPLER = 0,
        CONSTANT_BUFFER,
        COUNT,
    };

    std::uint32_t getBinding(BindingType t, std::uint32_t slot) const { return bindings_[slot][unsigned(t)]; }

    bool create(const uxs::db::value& config);

    VkPipelineLayout operator~() { return pipeline_layout_; }
    VkDescriptorSetLayout getDescriptorSetLayout(std::uint32_t layout_index) {
        return descriptor_set_layouts_[layout_index];
    }

    //@{ IUnknown
    util::ref_counter& getRefCounter() override { return *this; }
    //@}

    //@{ IPipelineLayout
    //@}

 private:
    util::reference<Device> device_;
    uxs::inline_dynarray<VkDescriptorSetLayout> descriptor_set_layouts_;
    VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};

    uxs::inline_dynarray<std::array<std::uint32_t, unsigned(BindingType::COUNT)>> bindings_;

    void setBinding(BindingType t, std::uint32_t slot, std::uint32_t binding) {
        if (slot >= bindings_.size()) { bindings_.resize(slot + 1); }
        bindings_[slot][unsigned(t)] = binding;
    }
};

}  // namespace app3d::rel::vulkan
