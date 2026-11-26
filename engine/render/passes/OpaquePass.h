#ifndef D3D12_ENGINE_OPAQUEPASS_H
#define D3D12_ENGINE_OPAQUEPASS_H

// Draws the solid surfaces, lit and shadowed, with holes where a cut-out material's alpha is below its cutoff.
// Every pipeline it needs is created up front, so no frame waits for one.

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

namespace render {
    class OpaquePass {
    public:
        explicit OpaquePass(rhi::PipelineCache &pipelines);

        OpaquePass(const OpaquePass &) = delete;
        OpaquePass &operator=(const OpaquePass &) = delete;
        OpaquePass(OpaquePass &&) = delete;
        OpaquePass &operator=(OpaquePass &&) = delete;

        void AddTo(FrameGraph &graph, FgTexture color, uint32_t colorView, FgTexture depth, FgTexture shadowMap,
                   const GpuResources &resources) const;

    private:
        struct PipelineKey {
            VertexFormat format = VertexFormat::Mesh;
            Shading shading = Shading::Lit;
            bool doubleSided = false;
            bool alphaTest = false;

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
        std::array<rhi::PipelineHandle, kPipelineKeyCount> m_pipelinesByKey;
    };
} // namespace render

#endif // D3D12_ENGINE_OPAQUEPASS_H
