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
    bool obtainDescriptorSet(std::uint32_t layout_index, VkDescriptorSet& descriptor_set);
    void releaseDescriptorSet(VkDescriptorSet descriptor_set);

    VkPipelineLayout operator~() { return pipeline_layout_; }

    //@{ IPipelineLayout
    util::ref_counter& getRefCounter() override { return *this; }
    //@}

 private:
    util::ref_ptr<Device> device_;
    uxs::inline_dynarray<VkDescriptorSetLayout> descriptor_set_layouts_;
    VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
    VkDescriptorPool descriptor_pool_{VK_NULL_HANDLE};

    bool createDescriptorPool(std::uint32_t max_sets, std::span<const VkDescriptorPoolSize> descriptor_types,
                              VkDescriptorPool& descriptor_pool);

    uxs::inline_dynarray<std::array<std::uint32_t, unsigned(BindingType::COUNT)>> bindings_;

    void setBinding(BindingType t, std::uint32_t slot, std::uint32_t binding) {
        if (slot >= bindings_.size()) { bindings_.resize(slot + 1); }
        bindings_[slot][unsigned(t)] = binding;
    }
};

}  // namespace app3d::rel::vulkan
