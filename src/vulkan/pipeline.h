#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

#include <uxs/dynarray.h>

#include <array>

namespace app3d::rel::vulkan {

class Device;
class RenderTarget;
class PipelineLayout;

class Pipeline final : public util::ref_counter, public IPipeline {
 public:
    Pipeline(Device& device, RenderTarget& render_target, PipelineLayout& pipeline_layout);
    ~Pipeline() override;

    enum class BindingType {
        VERTEX_BUFFER = 0,
        COUNT,
    };

    std::uint32_t getBinding(BindingType t, std::uint32_t slot) const { return bindings_[slot][unsigned(t)]; }

    bool create(std::span<IShaderModule* const> shader_modules, const uxs::db::value& config);

    VkPipeline operator~() { return pipeline_; }
    PipelineLayout& getLayout() { return *pipeline_layout_; }

    //@{ IPipeline
    util::ref_counter& getRefCounter() override { return *this; }
    //@}

 private:
    util::ref_ptr<Device> device_;
    util::ref_ptr<RenderTarget> render_target_;
    util::ref_ptr<PipelineLayout> pipeline_layout_;
    VkPipeline pipeline_{VK_NULL_HANDLE};

    uxs::inline_dynarray<std::array<std::uint32_t, unsigned(BindingType::COUNT)>> bindings_;

    void setBinding(BindingType t, std::uint32_t slot, std::uint32_t binding) {
        if (slot >= bindings_.size()) { bindings_.resize(slot + 1); }
        bindings_[slot][unsigned(t)] = binding;
    }
};

}  // namespace app3d::rel::vulkan
