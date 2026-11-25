#include "render/passes/TransparentPass.h"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>

#include "render/FrameInputs.h"
#include "render/passes/DrawConstantPacking.h"
#include "render/passes/ShadowPass.h"
#include "render/passes/VertexLayout.h"
#include "rhi/CommandContext.h"
#include "rhi/DepthFormat.h"
#include "rhi/SwapChain.h"

namespace render {
    TransparentPass::TransparentPass(rhi::PipelineCache &pipelines) : m_cache(pipelines), m_pipelines{} {
        constexpr std::array kBackBufferTargets{rhi::SwapChain::kRtvFormat};
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
                                FgTexture shadowMap, const GpuResources &resources) const {
        graph.AddPass(
            "Transparent",
            [&](PassBuilder &builder) {
                builder.WriteColor(color, std::nullopt, colorView);
                // Declared as a depth write, though the PSO only tests depth
                builder.WriteDepth(depth, std::nullopt);
                builder.Read(shadowMap);
            },
            [this, &resources, shadowMap](PassContext &context) {
                BindShadowConstants(context, context.ShaderIndex(shadowMap));
                Record(context.Commands(), resources, context.Inputs().transparentObjects);
            });
    }

    void TransparentPass::Record(rhi::CommandContext &context, const GpuResources &resources,
                                 std::span<const TransparentObject> objects) const {
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
            if (mesh.geometry != currentGeometry) {
                context.SetVertexBuffer(geometry.vertices.VertexBufferView(geometry.vertexStride));
                context.SetIndexBuffer(geometry.indices.IndexBufferView(geometry.indexFormat));
                currentGeometry = mesh.geometry;
            }

            context.SetGraphicsRootConstants(gpu::kRootDrawConstants,
                                             gpu::DrawConstants{.instanceOffset = object.element,
                                                                .materialIndex = material.albedo.index,
                                                                .instanceBufferIndex = object.instanceBufferIndex,
                                                                .alphaInfo = PackAlphaInfo(material)});
            if (material.doubleSided) {
                drawWith(Pipeline(geometry.vertexFormat, true), mesh);
            }
            drawWith(Pipeline(geometry.vertexFormat, false), mesh);
        }
    }
} // namespace render
