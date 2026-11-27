#include "render/passes/OpaquePass.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

#include "core/Assert.h"
#include "render/FrameInputs.h"
#include "render/passes/MeshDrawing.h"
#include "render/passes/ShadowPass.h"
#include "render/passes/VertexLayout.h"
#include "rhi/DepthFormat.h"
#include "rhi/SwapChain.h"

namespace render {
    OpaquePass::OpaquePass(rhi::PipelineCache &pipelines) : m_pipelines(pipelines), m_pipelinesByKey{} {
        constexpr std::array kBackBufferTargets{rhi::SwapChain::kRtvFormat};

        struct Variant {
            PipelineKey key;
            std::wstring_view pixelShader;
            std::wstring_view name;
        };
        // only combinations that exist, unlit has no alpha test
        constexpr std::array kVariants{
            Variant{{.shading = Shading::Lit}, L"ps_light.cso", L"Lit PSO"},
            Variant{{.shading = Shading::Lit, .doubleSided = true}, L"ps_light.cso", L"Lit two-sided PSO"},
            Variant{{.shading = Shading::Lit, .alphaTest = true}, L"ps_light_alpha_test.cso", L"Lit alpha-test PSO"},
            Variant{{.shading = Shading::Lit, .doubleSided = true, .alphaTest = true},
                    L"ps_light_alpha_test.cso",
                    L"Lit two-sided alpha-test PSO"},
            Variant{{.shading = Shading::Unlit}, L"ps_unlit.cso", L"Unlit PSO"},
            Variant{{.shading = Shading::Unlit, .doubleSided = true}, L"ps_unlit.cso", L"Unlit two-sided PSO"},
        };
        for (const Variant &variant : kVariants) {
            m_pipelinesByKey[variant.key.Index()] = pipelines.Get({
                .vertexShader = L"vs_mesh.cso",
                .pixelShader = variant.pixelShader,
                .inputLayout = kVertexLayout,
                .cull = variant.key.doubleSided ? rhi::CullMode::None : rhi::CullMode::Back,
                .renderTargetFormats = kBackBufferTargets,
                .depthFormat = rhi::kDepthFormat,
                .name = variant.name,
            });
        }
    }

    void OpaquePass::AddTo(FrameGraph &graph, FgTexture color, uint32_t colorView, FgTexture depth, FgTexture shadowMap,
                           const GpuResources &resources) const {
        graph.AddPass(
            "Opaque",
            [&](PassBuilder &builder) {
                builder.WriteColor(color, ClearColor{0.04f, 0.06f, 0.15f, 1.0f}, colorView);
                builder.WriteDepth(depth, rhi::kClearDepth);
                builder.Read(shadowMap);
            },
            [this, &resources, shadowMap](PassContext &context) {
                BindShadowConstants(context, context.ShaderIndex(shadowMap));
                const FrameInputs &inputs = context.Inputs();
                RecordMeshDraws(context.Commands(), m_pipelines, resources, inputs.drawPath, inputs.drawSets,
                                [this](VertexFormat format, const MaterialInfo &material) {
                                    if (material.alphaMode == AlphaMode::Blend) {
                                        return rhi::PipelineHandle{};
                                    }
                                    return PipelineFor(format, material);
                                });
            });
    }

    rhi::PipelineHandle OpaquePass::PipelineFor(VertexFormat format, const MaterialInfo &material) const {
        const PipelineKey key{.format = format,
                              .shading = material.shading,
                              .doubleSided = material.doubleSided,
                              .alphaTest = material.shading == Shading::Lit && material.alphaMode == AlphaMode::Mask};
        const rhi::PipelineHandle pipeline = m_pipelinesByKey[key.Index()];
        ENGINE_ASSERT_MSG(pipeline.IsValid(), "No PSO for vertex format {}, shading {}, alpha test {}",
                          std::to_underlying(format), std::to_underlying(material.shading), key.alphaTest);
        return pipeline;
    }
} // namespace render
