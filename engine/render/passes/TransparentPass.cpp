#include "render/passes/TransparentPass.h"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>

#include "core/Assert.h"
#include "render/FrameInputs.h"
#include "render/passes/MeshDrawing.h"
#include "rhi/CommandContext.h"
#include "rhi/DepthFormat.h"
#include "rhi/SwapChain.h"

namespace engine::render {
    TransparentPass::TransparentPass(rhi::PipelineCache &pipelines) : m_cache(pipelines), m_pipelines{} {
        constexpr std::array kBackBufferTargets{rhi::SwapChain::kRtvFormat};
        // Blended over the target, depth tested but not written, and the pixel shader build that outputs alpha
        for (const bool farSide : {false, true}) {
            m_pipelines[PipelineIndex(VertexFormat::Mesh, farSide)] = pipelines.Get({
                .vertexShader = L"vs_mesh.cso",
                .pixelShader = L"ps_light_blend.cso",
                .inputLayout = kVertexLayout,
                .blend = rhi::BlendMode::AlphaBlend,
                .cull = farSide ? rhi::CullMode::Front : rhi::CullMode::Back,
                .depth = rhi::DepthMode::TestOnly,
                .renderTargetFormats = kBackBufferTargets,
                .depthFormat = rhi::kDepthFormat,
                .name = farSide ? L"Transparent far-side PSO" : L"Transparent PSO",
            });
        }
    }

    void TransparentPass::AddTo(FrameGraph &graph, FgTexture color, uint32_t colorView, FgTexture depth,
                                const GpuResources &resources) const {
        graph.AddPass(
            "Transparent",
            [&](PassBuilder &builder) {
                // Over the opaque pass's picture and depth: nothing is cleared
                builder.WriteColor(color, std::nullopt, colorView);
                // Declared as a depth write, though the PSO only tests depth
                builder.WriteDepth(depth, std::nullopt);
            },
            [this, &resources](PassContext &context) {
                Record(context.Commands(), resources, context.Inputs().transparentObjects);
            });
    }

    void TransparentPass::Record(rhi::CommandContext &context, const GpuResources &resources,
                                 std::span<const TransparentObject> objects) const {
        if (objects.empty()) {
            return;
        }
        context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        rhi::PipelineHandle currentPipeline;
        uint32_t currentGeometry = std::numeric_limits<uint32_t>::max();
        const auto drawWith = [&](rhi::PipelineHandle pipeline, const GpuMesh &mesh) {
            if (pipeline != currentPipeline) {
                context.SetPipelineState(m_cache.Native(pipeline));
                currentPipeline = pipeline;
            }
            context.DrawIndexed(mesh.indexCount, mesh.firstIndex, mesh.baseVertex);
        };

        for (const TransparentObject &object : objects) {
            const GpuMesh &mesh = resources.Mesh(object.key.mesh);
            const GpuGeometry &geometry = resources.Geometry(mesh.geometry);
            const MaterialInfo &material = resources.Material(object.key.material);
            ENGINE_ASSERT_MSG(material.alphaMode == AlphaMode::Blend && material.shading == Shading::Lit,
                              "The transparent pass draws lit Blend materials only");
            if (mesh.geometry != currentGeometry) {
                context.SetVertexBuffer(geometry.vertices.VertexBufferView(geometry.vertexStride));
                context.SetIndexBuffer(geometry.indices.IndexBufferView(geometry.indexFormat));
                currentGeometry = mesh.geometry;
            }

            // One object per draw: its element in the instance buffer goes into the root constants (b0)
            context.SetGraphicsRootConstants(gpu::kRootDrawConstants,
                                             gpu::DrawConstants{.instanceOffset = object.element,
                                                                .materialIndex = material.albedo.index,
                                                                .instanceBufferIndex = object.instanceBufferIndex,
                                                                .alphaInfo = PackAlphaInfo(material)});
            // The far side first, so the near side blends over it. In D3D12 the cull mode is part of the PSO, so a
            // two-sided object switches PSOs twice: a few microseconds of CPU time each. Culling still saves the GPU
            // from shading the side a draw does not want.
            if (material.doubleSided) {
                drawWith(Pipeline(geometry.vertexFormat, true), mesh);
            }
            drawWith(Pipeline(geometry.vertexFormat, false), mesh);
        }
    }
} // namespace engine::render
