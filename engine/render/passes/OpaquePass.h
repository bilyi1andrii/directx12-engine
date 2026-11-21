#ifndef D3D12_ENGINE_OPAQUEPASS_H
#define D3D12_ENGINE_OPAQUEPASS_H

#include <array>
#include <cstdint>
#include <span>
#include <utility>

#include "render/FrameGraph.h"
#include "render/GpuResources.h"
#include "renderdata/InstanceChunks.h"
#include "renderdata/Material.h"
#include "renderdata/RendererSettings.h"
#include "rhi/PipelineCache.h"

namespace engine::rhi {
    class CommandContext;
} // namespace engine::rhi

namespace engine::render {
    // Draws solid surfaces into the bound colour and depth targets, with depth test and depth write on.
    // Owns one PSO per vertex format, shading and cull mode, created up front so no frame waits for a PSO.
    //
    // Record only sets PSOs, buffers and root constants (b0) and draws. The caller binds the render targets,
    // viewport, root signature and frame constants (b1) first; in a frame graph, the graph does.
    //
    // Not copyable or movable: it keeps a reference to `pipelines`, which must outlive it.
    class OpaquePass {
    public:
        explicit OpaquePass(rhi::PipelineCache &pipelines);

        OpaquePass(const OpaquePass &) = delete;
        OpaquePass &operator=(const OpaquePass &) = delete;
        OpaquePass(OpaquePass &&) = delete;
        OpaquePass &operator=(OpaquePass &&) = delete;

        // Adds the pass to `graph`: it clears `color` to the sky colour and `depth` to the far plane, then records
        // FrameInputs::drawSets. It draws through `color`'s view number `colorView`, which must be in
        // SwapChain::kRtvFormat. The pass and `resources` must outlive the graph's passes.
        void AddTo(FrameGraph &graph, FgTexture color, uint32_t colorView, FgTexture depth,
                   const GpuResources &resources) const;

        // Records every draw of `drawSets`, in order. The draws read meshes and materials from `resources`.
        // On the instanced path each draw is one DrawIndexedInstanced; otherwise one DrawIndexed per object.
        void Record(rhi::CommandContext &context, const GpuResources &resources, DrawPath drawPath,
                    std::span<const std::span<const InstanceDraw>> drawSets) const;

    private:
        [[nodiscard]] rhi::PipelineHandle PipelineFor(VertexFormat format, const MaterialInfo &material) const;

        const rhi::PipelineCache &m_pipelines;
        // PSOs by [vertex format][shading][doubleSided ? 1 : 0], all filled by the constructor
        using ShadingPipelines = std::array<std::array<rhi::PipelineHandle, 2>, kShadingCount>;
        std::array<ShadingPipelines, std::to_underlying(VertexFormat::Count)> m_pipelinesByFormat;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_OPAQUEPASS_H
