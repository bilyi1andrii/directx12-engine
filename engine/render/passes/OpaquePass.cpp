#include "render/passes/OpaquePass.h"

#include <array>
#include <cstdint>
#include <limits>
#include <ranges>
#include <string_view>
#include <utility>

#include "core/Assert.h"
#include "render/FrameInputs.h"
#include "render/passes/DrawConstantPacking.h"
#include "render/passes/VertexLayout.h"
#include "rhi/CommandContext.h"
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

    void OpaquePass::AddTo(FrameGraph &graph, FgTexture color, uint32_t colorView, FgTexture depth,
                           const GpuResources &resources) const {
        graph.AddPass(
            "Opaque",
            [&](PassBuilder &builder) {
                builder.WriteColor(color, ClearColor{0.04f, 0.06f, 0.15f, 1.0f}, colorView);
                // Every pixel starts at the far plane, so the first triangle drawn there passes the depth test
                builder.WriteDepth(depth, rhi::kClearDepth);
            },
            [this, &resources](PassContext &context) {
                const FrameInputs &inputs = context.Inputs();
                Record(context.Commands(), resources, inputs.drawPath, inputs.drawSets);
            });
    }

    void OpaquePass::Record(rhi::CommandContext &context, const GpuResources &resources, DrawPath drawPath,
                            std::span<const std::span<const InstanceDraw>> drawSets) const {
        context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        // Root constants (b0) are copied into the command list, so every draw keeps its own values.
        // The PSO and buffers carry over from one set to the next: geometry indices are the same in every set.
        rhi::PipelineHandle currentPipeline;
        uint32_t currentGeometry = std::numeric_limits<uint32_t>::max();
        for (const InstanceDraw &draw : drawSets | std::views::join) {
            const MaterialInfo &material = resources.Material(draw.key.material);
            // Transparent objects are drawn later, one by one and sorted, by the transparent pass
            if (material.alphaMode == AlphaMode::Blend) {
                continue;
            }
            const GpuMesh &mesh = resources.Mesh(draw.key.mesh);
            const GpuGeometry &geometry = resources.Geometry(mesh.geometry);
            // Switch PSOs only when needed: it costs CPU and GPU time. The PSO depends on the geometry's vertex
            // format and on the material, so it is checked for every draw.
            const rhi::PipelineHandle pipeline = PipelineFor(geometry.vertexFormat, material);
            if (pipeline != currentPipeline) {
                context.SetPipelineState(m_pipelines.Native(pipeline));
                currentPipeline = pipeline;
            }
            // The meshes of one model share their buffers, and a bucket split between two chunks gives two draws
            // of the same mesh: bind the buffers only when the geometry changes
            if (mesh.geometry != currentGeometry) {
                context.SetVertexBuffer(geometry.vertices.VertexBufferView(geometry.vertexStride));
                context.SetIndexBuffer(geometry.indices.IndexBufferView(geometry.indexFormat));
                currentGeometry = mesh.geometry;
            }

            // Every draw reads indexCount indices from firstIndex on, and the GPU adds baseVertex to each
            gpu::DrawConstants constants{.instanceOffset = draw.firstElement,
                                         .materialIndex = material.albedo.index,
                                         .instanceBufferIndex = draw.instanceBufferIndex,
                                         .alphaInfo = PackAlphaInfo(material)};
            if (drawPath == DrawPath::Instanced) {
                // One draw for all `count` objects: object i reads element instanceOffset + SV_InstanceID (= i)
                context.SetGraphicsRootConstants(gpu::kRootDrawConstants, constants);
                context.DrawIndexedInstanced(mesh.indexCount, draw.count, mesh.firstIndex, mesh.baseVertex);
            } else {
                // One draw per object, each with its own element in the root constants: this keeps the
                // per-draw cost measurable. The material and buffer are the same for all of them, so only
                // instanceOffset is sent per draw.
                context.SetGraphicsRootConstants(gpu::kRootDrawConstants, constants);
                for (uint32_t i = 0; i < draw.count; ++i) {
                    context.SetGraphicsRootConstant(gpu::kRootDrawConstants, draw.firstElement + i,
                                                    kInstanceOffsetConstant);
                    context.DrawIndexed(mesh.indexCount, mesh.firstIndex, mesh.baseVertex);
                }
            }
        }
    }

    rhi::PipelineHandle OpaquePass::PipelineFor(VertexFormat format, const MaterialInfo &material) const {
        ENGINE_ASSERT_MSG(material.alphaMode != AlphaMode::Blend, "Transparent materials have no opaque PSO");
        // The unlit shader has no alpha-test build, so an unlit cut-out material is drawn solid (Debug builds reject
        // it when it is made)
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
