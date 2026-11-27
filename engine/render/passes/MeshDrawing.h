#ifndef D3D12_ENGINE_MESHDRAWING_H
#define D3D12_ENGINE_MESHDRAWING_H

// The draw loop the shadow and opaque passes share: it picks a pipeline per material and draws each group of
// objects instanced, or one object at a time.

#include <cstdint>
#include <limits>
#include <ranges>
#include <span>

#include "render/GpuResources.h"
#include "render/passes/DrawConstantPacking.h"
#include "renderdata/InstanceChunks.h"
#include "renderdata/Material.h"
#include "renderdata/RendererSettings.h"
#include "rhi/CommandContext.h"
#include "rhi/PipelineCache.h"

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"

namespace render {
    // pipelineFor returns invalid handle to skip draw
    template <typename PipelineFor>
    void RecordMeshDraws(rhi::CommandContext &context, const rhi::PipelineCache &pipelines,
                         const GpuResources &resources, DrawPath drawPath,
                         std::span<const std::span<const InstanceDraw>> drawSets, PipelineFor pipelineFor) {
        context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        rhi::PipelineHandle currentPipeline;
        uint32_t currentGeometry = std::numeric_limits<uint32_t>::max();
        for (const InstanceDraw &draw : drawSets | std::views::join) {
            const GpuMesh &mesh = resources.Mesh(draw.key.mesh);
            const GpuGeometry &geometry = resources.Geometry(mesh.geometry);
            const MaterialInfo &material = resources.Material(draw.key.material);
            const rhi::PipelineHandle pipeline = pipelineFor(geometry.vertexFormat, material);
            if (!pipeline.IsValid()) {
                continue;
            }
            // switch PSO and buffers only when they change
            if (pipeline != currentPipeline) {
                context.SetPipelineState(pipelines.Native(pipeline));
                currentPipeline = pipeline;
            }
            if (mesh.geometry != currentGeometry) {
                context.SetVertexBuffer(geometry.vertices.VertexBufferView(geometry.vertexStride));
                context.SetIndexBuffer(geometry.indices.IndexBufferView(geometry.indexFormat));
                currentGeometry = mesh.geometry;
            }

            gpu::DrawConstants constants{.instanceOffset = draw.firstElement,
                                         .materialIndex = material.albedo.index,
                                         .instanceBufferIndex = draw.instanceBufferIndex,
                                         .alphaInfo = PackAlphaInfo(material)};
            context.SetGraphicsRootConstants(gpu::kRootDrawConstants, constants);
            if (drawPath == DrawPath::Instanced) {
                context.DrawIndexedInstanced(mesh.indexCount, draw.count, mesh.firstIndex, mesh.baseVertex);
            } else {
                // one draw per object, only instanceOffset changes
                for (uint32_t i = 0; i < draw.count; ++i) {
                    context.SetGraphicsRootConstant(gpu::kRootDrawConstants, draw.firstElement + i,
                                                    kInstanceOffsetConstant);
                    context.DrawIndexed(mesh.indexCount, mesh.firstIndex, mesh.baseVertex);
                }
            }
        }
    }
} // namespace render

#endif // D3D12_ENGINE_MESHDRAWING_H
