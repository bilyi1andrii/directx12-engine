#ifndef D3D12_ENGINE_DYNAMICINSTANCES_H
#define D3D12_ENGINE_DYNAMICINSTANCES_H

#include <span>
#include <vector>

#include <DirectXMath.h>

#include "renderdata/InstanceChunks.h"
#include "renderdata/RenderList.h"
#include "renderdata/TransparentDraws.h"

namespace engine::rhi {
    class BindlessHeap;
    class FrameContext;
} // namespace engine::rhi

namespace engine::render {
    class GpuResources;

    // The instance data of objects that move, written again every frame, and the draws that read it.
    // Keeps its vectors between frames, so their memory is reused.
    class DynamicInstances {
    public:
        // Writes the world matrices of `list` into `frame`'s upload ring and replaces Draws(). Each chunk of at most
        // one ring page gets a new bindless view in `heap`, freed after `frame`'s fence.
        void Upload(const RenderList &list, rhi::FrameContext &frame, rhi::BindlessHeap &heap);

        // The draws of the last Upload, in the list's bucket order. Valid until the next Upload.
        [[nodiscard]] std::span<const InstanceDraw> Draws() const noexcept { return m_draws; }

        // Appends one draw to `draws` for every object of the last Upload with a transparent material, with its
        // depth seen through `view` (world -> view, row vectors). `list` must be the list of that Upload: the
        // positions come from its matrices, never from the upload memory, which is slow to read.
        void AppendTransparent(const RenderList &list, const GpuResources &resources, const DirectX::XMFLOAT4X4 &view,
                               std::vector<TransparentDraw> &draws) const;

    private:
        std::vector<InstanceRange> m_ranges; // the last Upload's ranges: m_ranges[i] holds the objects of m_draws[i]
        std::vector<InstanceDraw> m_draws;
    };
} // namespace engine::render

#endif // D3D12_ENGINE_DYNAMICINSTANCES_H
