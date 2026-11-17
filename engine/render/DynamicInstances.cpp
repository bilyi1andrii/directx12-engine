#include "render/DynamicInstances.h"

#include <cstdint>

#include "render/InstanceUpload.h"
#include "rhi/BindlessHeap.h"
#include "rhi/FrameContext.h"
#include "rhi/UploadRing.h"

namespace engine::render {
    void DynamicInstances::Upload(const RenderList &list, rhi::FrameContext &frame, rhi::BindlessHeap &heap) {
        const auto beginChunk = [&](uint32_t elementCount) {
            // Aligned to the element size, so the view can start at a whole element
            InstanceChunk chunk{.memory =
                                    frame.Ring().Allocate(uint64_t{elementCount} * kInstanceStride, kInstanceStride)};
            // A fresh bindless slot every frame: the previous frame may still read the old one. It is freed after
            // this frame's fence.
            chunk.instanceBufferIndex = heap.Allocate();
            frame.DeferFree(heap, chunk.instanceBufferIndex);
            heap.CreateStructuredBufferSrv(chunk.instanceBufferIndex, chunk.memory.resource,
                                           chunk.memory.offset / kInstanceStride, elementCount, kInstanceStride);
            return chunk;
        };
        // A shader view (SRV) covers one buffer, and every ring page is its own buffer: chunks of at most a page
        m_draws.clear();
        WriteInstances(list, static_cast<uint32_t>(frame.Ring().PageSize() / kInstanceStride), beginChunk, m_ranges,
                       m_draws);
    }
} // namespace engine::render
