#ifndef D3D12_ENGINE_SHADOWPASS_H
#define D3D12_ENGINE_SHADOWPASS_H

#include <cstdint>

#include "render/FrameGraph.h"
#include "render/GpuResources.h"
#include "rhi/PipelineCache.h"

namespace render {
    class ShadowPass {
    public:
        explicit ShadowPass(rhi::PipelineCache &pipelines);

        ShadowPass(const ShadowPass &) = delete;
        ShadowPass &operator=(const ShadowPass &) = delete;
        ShadowPass(ShadowPass &&) = delete;
        ShadowPass &operator=(ShadowPass &&) = delete;

        void AddTo(FrameGraph &graph, FgTexture shadowMap, const GpuResources &resources) const;

    private:
        const rhi::PipelineCache &m_pipelines;
        rhi::PipelineHandle m_culled;
        rhi::PipelineHandle m_twoSided;
    };

    void BindShadowConstants(PassContext &context, uint32_t shadowMapIndex);
} // namespace render

#endif // D3D12_ENGINE_SHADOWPASS_H
