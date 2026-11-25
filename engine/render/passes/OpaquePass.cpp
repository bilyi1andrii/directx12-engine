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

namespace engine::render {
    OpaquePass::OpaquePass(rhi::PipelineCache &pipelines) : m_pipelines(pipelines), m_pipelinesByKey{} {
        constexpr std::array kBackBufferTargets{rhi::SwapChain::kRtvFormat};

        // A PSO fixes the pixel shader and the cull mode; a command list cannot change them between draws.
        // So each combination a material can ask for gets its own PSO. The alpha test is a second build of the lit
        // shader. A shader that may discard pixels stops the GPU from testing depth before it runs, so solid
        // surfaces keep the build without it.
        struct Variant {
            PipelineKey key; // all variants draw meshes in the one vertex format there is
            std::wstring_view pixelShader;
            std::wstring_view name;
        };
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
            rhi::PipelineHandle &slot = m_pipelinesByKey[variant.key.Index()];
            ENGINE_ASSERT_MSG(!slot.IsValid(), "Two PSO variants share index {}", variant.key.Index());
            slot = pipelines.Get({
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
                // Every pixel starts at the far plane, so the first triangle drawn there passes the depth test
                builder.WriteDepth(depth, rhi::kClearDepth);
                builder.Read(shadowMap);
            },
            [this, &resources, shadowMap](PassContext &context) {
                BindShadowConstants(context, context.ShaderIndex(shadowMap));
                const FrameInputs &inputs = context.Inputs();
                RecordMeshDraws(context.Commands(), m_pipelines, resources, inputs.drawPath, inputs.drawSets,
                                [this](VertexFormat format, const MaterialInfo &material) {
                                    // TransparentPass draws these
                                    if (material.alphaMode == AlphaMode::Blend) {
                                        return rhi::PipelineHandle{};
                                    }
                                    return PipelineFor(format, material);
                                });
            });
    }

    rhi::PipelineHandle OpaquePass::PipelineFor(VertexFormat format, const MaterialInfo &material) const {
        // Unlit has no alpha-test build; CreateMaterial rejects unlit cut-out materials in Debug
        const PipelineKey key{.format = format,
                              .shading = material.shading,
                              .doubleSided = material.doubleSided,
                              .alphaTest = material.shading == Shading::Lit && material.alphaMode == AlphaMode::Mask};
        const rhi::PipelineHandle pipeline = m_pipelinesByKey[key.Index()];
        ENGINE_ASSERT_MSG(pipeline.IsValid(), "No PSO for vertex format {}, shading {}, alpha test {}",
                          std::to_underlying(format), std::to_underlying(material.shading), key.alphaTest);
        return pipeline;
    }
} // namespace engine::render
