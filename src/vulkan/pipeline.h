#pragma once

#include "vulkan_api.h"

#include "interfaces/i_rendering_driver.h"

#include <uxs/dynarray.h>

#include <array>

namespace app3d::rel::vulkan {

class Device;
class PipelineLayout;
class RenderTarget;

class Pipeline final : public IPipeline {
 public:
    Pipeline(Device& device, PipelineLayout& pipeline_layout);
    ~Pipeline() override;
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    enum class BindingType {
        VERTEX_BUFFER = 0,
        COUNT,
    };

    std::uint32_t getBinding(BindingType t, std::uint32_t slot) const { return bindings_[slot][unsigned(t)]; }

    bool create(RenderTarget& render_target, std::span<IShaderModule* const> shader_modules,
                const uxs::db::value& config);

    VkPipeline operator~() { return pipeline_; }
    PipelineLayout& getLayout() { return pipeline_layout_; }

    //@{ IPipeline
    //@}

 private:
    Device& device_;
    PipelineLayout& pipeline_layout_;
    VkPipeline pipeline_{VK_NULL_HANDLE};

    uxs::inline_dynarray<std::array<std::uint32_t, unsigned(BindingType::COUNT)>> bindings_;

    void setBinding(BindingType t, std::uint32_t slot, std::uint32_t binding) {
        if (slot >= bindings_.size()) { bindings_.resize(slot + 1); }
        bindings_[slot][unsigned(t)] = binding;
    }
};

}  // namespace app3d::rel::vulkan
