#ifndef D3D12_ENGINE_OPAQUEPASS_H
#define D3D12_ENGINE_OPAQUEPASS_H

#include <array>
#include <cstddef>
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
    // Draws solid surfaces into the bound colour and depth targets, with depth test and depth write on. Mask
    // materials get holes where their alpha is below the cutoff (alpha test).
    // Owns one PSO per vertex format, shading, cull mode and alpha test, created up front so no frame waits for a PSO.
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

        // Records every draw of `drawSets`, in order, except those with a transparent (Blend) material. The draws read
        // meshes and materials from `resources`. On the instanced path each draw is one DrawIndexedInstanced;
        // otherwise one DrawIndexed per object.
        void Record(rhi::CommandContext &context, const GpuResources &resources, DrawPath drawPath,
                    std::span<const std::span<const InstanceDraw>> drawSets) const;

    private:
        // Everything that picks one of the pass's PSOs
        struct PipelineKey {
            VertexFormat format = VertexFormat::Mesh;
            Shading shading = Shading::Lit;
            bool doubleSided = false;
            bool alphaTest = false;

            // A different slot in the PSO array for every combination
            [[nodiscard]] constexpr uint32_t Index() const noexcept {
                const uint32_t formatAndShading =
                    std::to_underlying(format) * uint32_t{std::to_underlying(Shading::Count)} +
                    std::to_underlying(shading);
                return (formatAndShading * 2 + (doubleSided ? 1 : 0)) * 2 + (alphaTest ? 1 : 0);
            }
        };
        static constexpr size_t kPipelineKeyCount =
            size_t{std::to_underlying(VertexFormat::Count)} * std::to_underlying(Shading::Count) * 2 * 2;

        [[nodiscard]] rhi::PipelineHandle PipelineFor(VertexFormat format, const MaterialInfo &material) const;

        const rhi::PipelineCache &m_pipelines;
        // One slot per key, filled by the constructor. Unlit materials have no alpha-test PSO.
        std::array<rhi::PipelineHandle, kPipelineKeyCount> m_pipelinesByKey;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_OPAQUEPASS_H
