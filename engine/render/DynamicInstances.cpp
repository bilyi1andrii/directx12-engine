#include "render/DynamicInstances.h"

#include <cstddef>
#include <cstdint>

#include "render/GpuResources.h"
#include "render/InstanceUpload.h"
#include "rhi/BindlessHeap.h"
#include "rhi/FrameContext.h"
#include "rhi/UploadRing.h"

namespace render {
    void DynamicInstances::Upload(const RenderList &list, rhi::FrameContext &frame, rhi::BindlessHeap &heap) {
        const auto beginChunk = [&](uint32_t elementCount) {
            InstanceChunk chunk{.memory =
                                    frame.Ring().Allocate(uint64_t{elementCount} * kInstanceStride, kInstanceStride)};
            chunk.instanceBufferIndex = heap.Allocate();
            frame.DeferFree(heap, chunk.instanceBufferIndex);
            heap.CreateStructuredBufferSrv(chunk.instanceBufferIndex, chunk.memory.resource,
                                           chunk.memory.offset / kInstanceStride, elementCount, kInstanceStride);
            return chunk;
        };
        m_draws.clear();
        WriteInstances(list, static_cast<uint32_t>(frame.Ring().PageSize() / kInstanceStride), beginChunk, m_ranges,
                       m_draws);
    }

    void DynamicInstances::AppendTransparent(const RenderList &list, const GpuResources &resources,
                                             const DirectX::XMFLOAT4X4 &view,
                                             std::vector<TransparentObject> &objects) const {
        const std::span<const BatchBucket> buckets = list.Buckets();
        for (size_t i = 0; i < m_draws.size(); ++i) {
            const InstanceDraw &draw = m_draws[i];
            if (resources.Material(draw.key.material).alphaMode != AlphaMode::Blend) {
                continue;
            }
            const InstanceRange &range = m_ranges[i];
            const BatchBucket &bucket = buckets[range.bucket];
            const DirectX::XMFLOAT3 &center = resources.Mesh(draw.key.mesh).center;
            for (uint32_t object = 0; object < range.count; ++object) {
                const DirectX::XMFLOAT4X4 &world = bucket.worlds[range.firstObject + object];
                objects.push_back({.key = draw.key,
                                   .instanceBufferIndex = draw.instanceBufferIndex,
                                   .element = draw.firstElement + object,
                                   .viewDepth = ViewDepth(view, SortPoint(world, center))});
            }
        }
    }
} // namespace render
