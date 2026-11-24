#ifndef D3D12_ENGINE_TRANSPARENTPASS_H
#define D3D12_ENGINE_TRANSPARENTPASS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "render/FrameGraph.h"
#include "render/GpuResources.h"
#include "renderdata/TransparentSort.h"
#include "rhi/PipelineCache.h"

namespace engine::rhi {
    class CommandContext;
} // namespace engine::rhi

namespace engine::render {
    // Draws transparent (Blend) surfaces over the solid ones, in the order given: back to front, so each one blends
    // over what lies behind it. Tests depth against the solid surfaces but does not write it, so a transparent
    // surface never hides another one behind it.
    // A two-sided material is drawn twice per object: first its far side, then its near side over it.
    // Owns one PSO per vertex format and culled side, created up front so no frame waits for a PSO.
    //
    // Record only sets PSOs, buffers and root constants (b0) and draws. In a frame graph, the graph binds the targets,
    // viewport, root signature and frame constants (b1).
    //
    // Not copyable or movable: it keeps a reference to `pipelines`, which must outlive it.
    class TransparentPass {
    public:
        explicit TransparentPass(rhi::PipelineCache &pipelines);

        TransparentPass(const TransparentPass &) = delete;
        TransparentPass &operator=(const TransparentPass &) = delete;
        TransparentPass(TransparentPass &&) = delete;
        TransparentPass &operator=(TransparentPass &&) = delete;

        // Adds the pass to `graph`: it draws FrameInputs::transparentObjects into `color` and tests them against
        // `depth`, keeping both as earlier passes left them. It draws through `color`'s view number `colorView`, which
        // must be in SwapChain::kRtvFormat. The pass and `resources` must outlive the graph's passes.
        void AddTo(FrameGraph &graph, FgTexture color, uint32_t colorView, FgTexture depth,
                   const GpuResources &resources) const;

        // Records `objects` in order, one DrawIndexed per object (two for a two-sided material), whatever the draw
        // path. The draws read meshes and materials from `resources`; every material must be lit and Blend.
        void Record(rhi::CommandContext &context, const GpuResources &resources,
                    std::span<const TransparentObject> objects) const;

    private:
        // Slot in m_pipelines of the PSO for meshes in `format` that draws only the triangles facing away (`farSide`)
        // or only those facing the camera
        [[nodiscard]] static constexpr size_t PipelineIndex(VertexFormat format, bool farSide) noexcept {
            return size_t{std::to_underlying(format)} * 2 + (farSide ? 1 : 0);
        }
        [[nodiscard]] rhi::PipelineHandle Pipeline(VertexFormat format, bool farSide) const {
            return m_pipelines[PipelineIndex(format, farSide)];
        }

        const rhi::PipelineCache &m_cache;
        // Two per vertex format: one for each side
        static constexpr size_t kPipelineCount = size_t{std::to_underlying(VertexFormat::Count)} * 2;
        std::array<rhi::PipelineHandle, kPipelineCount> m_pipelines;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_TRANSPARENTPASS_H
