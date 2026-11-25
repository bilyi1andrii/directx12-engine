#ifndef D3D12_ENGINE_TRANSPARENTPASS_H
#define D3D12_ENGINE_TRANSPARENTPASS_H

// The pass that draws see-through surfaces over the solid ones, from the farthest to the nearest, so each one
// blends over what lies behind it. A two-sided surface is drawn twice: its far side first, then its near side.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "render/FrameGraph.h"
#include "render/GpuResources.h"
#include "renderdata/TransparentSort.h"
#include "rhi/PipelineCache.h"

namespace rhi {
    class CommandContext;
} // namespace rhi

namespace render {
    class TransparentPass {
    public:
        explicit TransparentPass(rhi::PipelineCache &pipelines);

        TransparentPass(const TransparentPass &) = delete;
        TransparentPass &operator=(const TransparentPass &) = delete;
        TransparentPass(TransparentPass &&) = delete;
        TransparentPass &operator=(TransparentPass &&) = delete;

        void AddTo(FrameGraph &graph, FgTexture color, uint32_t colorView, FgTexture depth, FgTexture shadowMap,
                   const GpuResources &resources) const;

        void Record(rhi::CommandContext &context, const GpuResources &resources,
                    std::span<const TransparentObject> objects) const;

    private:
        [[nodiscard]] static constexpr size_t PipelineIndex(VertexFormat format, bool farSide) noexcept {
            return size_t{std::to_underlying(format)} * 2 + (farSide ? 1 : 0);
        }
        [[nodiscard]] rhi::PipelineHandle Pipeline(VertexFormat format, bool farSide) const {
            return m_pipelines[PipelineIndex(format, farSide)];
        }

        const rhi::PipelineCache &m_cache;
        static constexpr size_t kPipelineCount = size_t{std::to_underlying(VertexFormat::Count)} * 2;
        std::array<rhi::PipelineHandle, kPipelineCount> m_pipelines;
    };
} // namespace render

#endif // D3D12_ENGINE_TRANSPARENTPASS_H
