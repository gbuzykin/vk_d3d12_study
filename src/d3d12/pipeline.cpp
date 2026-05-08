#include "pipeline.h"

#include "d3d12_logger.h"
#include "device.h"
#include "pipeline_layout.h"
#include "render_target.h"
#include "shader_module.h"
#include "tables.h"

#include "rel/tables.h"

using namespace app3d;
using namespace app3d::rel;
using namespace app3d::rel::d3d12;

// --------------------------------------------------------
// Pipeline class implementation

Pipeline::Pipeline(Device& device, RenderTarget& render_target, PipelineLayout& pipeline_layout)
    : device_(util::not_null{&device}), render_target_(util::not_null{&render_target}),
      pipeline_layout_(util::not_null{&pipeline_layout}) {}

Pipeline::~Pipeline() {}

bool Pipeline::create(std::span<IShaderModule* const> shader_modules, const uxs::db::value& config) {
    const D3D12_RENDER_TARGET_BLEND_DESC default_blend{
        .BlendEnable = FALSE,
        .LogicOpEnable = FALSE,
        .SrcBlend = D3D12_BLEND_ONE,
        .DestBlend = D3D12_BLEND_ZERO,
        .BlendOp = D3D12_BLEND_OP_ADD,
        .SrcBlendAlpha = D3D12_BLEND_ONE,
        .DestBlendAlpha = D3D12_BLEND_ZERO,
        .BlendOpAlpha = D3D12_BLEND_OP_ADD,
        .LogicOp = D3D12_LOGIC_OP_NOOP,
        .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
    };

    const D3D12_DEPTH_STENCILOP_DESC default_stencil_op{
        .StencilFailOp = D3D12_STENCIL_OP_KEEP,
        .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
        .StencilPassOp = D3D12_STENCIL_OP_KEEP,
        .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS,
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc{
        .pRootSignature = pipeline_layout_->getD3D12RootSignature(),
        .BlendState =
            D3D12_BLEND_DESC{
                .AlphaToCoverageEnable = FALSE,
                .IndependentBlendEnable = FALSE,
                .RenderTarget = {default_blend},
            },
        .SampleMask = UINT_MAX,
        .RasterizerState =
            D3D12_RASTERIZER_DESC{
                .FillMode = D3D12_FILL_MODE_SOLID,
                .CullMode = D3D12_CULL_MODE_BACK,
                .FrontCounterClockwise = TRUE,
                .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
                .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
                .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
                .DepthClipEnable = TRUE,
                .MultisampleEnable = FALSE,
                .AntialiasedLineEnable = FALSE,
                .ForcedSampleCount = 0,
                .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
            },
        .DepthStencilState =
            D3D12_DEPTH_STENCIL_DESC{
                .DepthEnable = TRUE,
                .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
                .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
                .StencilEnable = FALSE,
                .StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
                .StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
                .FrontFace = default_stencil_op,
                .BackFace = default_stencil_op,
            },
        .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .NumRenderTargets = 1,
        .RTVFormats = {render_target_->getImageFormat()},
        .DSVFormat = render_target_->getDepthStencilFormat(),
        .SampleDesc = {.Count = 1, .Quality = 0},
    };

    // Shader stages :

    const auto& shader_stages = config.value("stages");

    std::uint32_t def_module_index = 0;
    for (const auto& module : shader_stages.as_array()) {
        const std::uint32_t index = module.value_or<std::uint32_t>("module_index", def_module_index);
        def_module_index = index + 1;
        if (index >= shader_modules.size()) { throw uxs::db::database_error("shader module index out of range"); }
        const auto& bytecode = static_cast<ShaderModule&>(*shader_modules[index]).getBytecode();
        const D3D12_SHADER_BYTECODE bytecode_desc{
            .pShaderBytecode = bytecode.getData(),
            .BytecodeLength = SIZE_T(bytecode.getSize()),
        };
        switch (parseShaderStage(module.value("stage").as_string_view())) {
            case ShaderStage::VERTEX_SHADER: pipeline_desc.VS = bytecode_desc; break;
            case ShaderStage::PIXEL_SHADER: pipeline_desc.PS = bytecode_desc; break;
            default: break;
        }
    }

    // Vertex layouts :

    uxs::inline_dynarray<D3D12_INPUT_ELEMENT_DESC> vertex_attribute_descriptions;

    const auto& vertex_layouts = config.value("vertex_layouts");

    std::uint32_t def_slot = 0;

    for (const auto& layout : vertex_layouts.as_array()) {
        const std::uint32_t slot = layout.value_or<std::uint32_t>("slot", def_slot);
        def_slot = slot + 1;

        const auto& attributes = layout.value("attributes");

        const auto align_up = [](auto v, auto alignment) { return (v + alignment - 1) & ~(alignment - 1); };

        std::uint32_t def_offset = 0;
        std::uint32_t max_alignment = 0;
        for (const auto& attribute : attributes.as_array()) {
            const auto format = parseFormat(attribute.value("format").as_string_view());

            const std::uint32_t attribute_alignment = TBL_FORMAT_ALIGNMENT[unsigned(format)];
            def_offset = align_up(def_offset, attribute_alignment);
            max_alignment = std::max(attribute_alignment, max_alignment);

            const std::uint32_t offset = attribute.value_or<std::uint32_t>("offset", def_offset);
            def_offset = offset + TBL_FORMAT_SIZE[unsigned(format)];

            vertex_attribute_descriptions.emplace_back(D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = attribute.value("name").as_c_string(),
                .SemanticIndex = attribute.value<UINT>("index"),
                .Format = TBL_D3D12_FORMAT[unsigned(format)],
                .InputSlot = UINT(slot),
                .AlignedByteOffset = UINT(offset),
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            });
        }

        const std::uint32_t stride = layout.value_or<std::uint32_t>("stride", align_up(def_offset, max_alignment));
        if (slot >= vertex_strides_.size()) { vertex_strides_.resize(slot + 1); }
        vertex_strides_[slot] = stride;
    }

    pipeline_desc.InputLayout = D3D12_INPUT_LAYOUT_DESC{
        .pInputElementDescs = vertex_attribute_descriptions.data(),
        .NumElements = UINT(vertex_attribute_descriptions.size()),
    };

    HRESULT result = device_->getD3D12Device()->CreateGraphicsPipelineState(
        &pipeline_desc, IID_PPV_ARGS(pipeline_.reset_and_get_address()));
    if (result != S_OK) {
        logError(LOG_D3D12 "couldn't create PSO: {}", D3D12Result{result});
        return false;
    }

    return true;
}

//@{ IPipeline

//@}
