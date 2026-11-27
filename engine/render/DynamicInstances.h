#ifndef D3D12_ENGINE_DYNAMICINSTANCES_H
#define D3D12_ENGINE_DYNAMICINSTANCES_H

// The instance data of moving objects, written into the frame's upload memory every frame,
// and the draws that read it.

#include <span>
#include <vector>

#include <DirectXMath.h>

#include "renderdata/InstanceChunks.h"
#include "renderdata/RenderList.h"
#include "renderdata/TransparentSort.h"

namespace rhi {
    class BindlessHeap;
    class FrameContext;
} // namespace rhi

namespace render {
    class GpuResources;

    class DynamicInstances {
    public:
        void Upload(const RenderList &list, rhi::FrameContext &frame, rhi::BindlessHeap &heap);

        [[nodiscard]] std::span<const InstanceDraw> Draws() const noexcept { return m_draws; }

        void AppendTransparent(const RenderList &list, const GpuResources &resources, const DirectX::XMFLOAT4X4 &view,
                               std::vector<TransparentObject> &objects) const;

    private:
        std::vector<InstanceRange> m_ranges;
        std::vector<InstanceDraw> m_draws; // one per range, same order
    };
} // namespace render

#endif // D3D12_ENGINE_DYNAMICINSTANCES_H
