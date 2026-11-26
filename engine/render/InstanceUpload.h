#ifndef D3D12_ENGINE_INSTANCEUPLOAD_H
#define D3D12_ENGINE_INSTANCEUPLOAD_H

// Writes objects' world matrices into instance buffers in chunks, and makes one draw per range of objects. Moving
// objects and static lists share it. Only the renderer's .cpp files include it.

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

#include <DirectXMath.h>

#include "renderdata/InstanceChunks.h"
#include "renderdata/RenderList.h"
#include "rhi/UploadRing.h"

#include "common/shared_types.hlsli"

namespace render {
    inline constexpr uint32_t kInstanceStride = sizeof(gpu::InstanceData);

    struct InstanceChunk {
        rhi::UploadAllocation memory;
        uint32_t instanceBufferIndex = 0; // bindless
    };

    template <typename BeginChunk>
    void WriteInstances(const RenderList &list, uint32_t chunkCapacity, BeginChunk &&beginChunk,
                        std::vector<InstanceRange> &ranges, std::vector<InstanceDraw> &draws) {
        SplitIntoChunks(list.Buckets(), chunkCapacity, ranges);
        draws.reserve(draws.size() + ranges.size());

        const std::span<const BatchBucket> buckets = list.Buckets();
        const uint64_t instanceCount = list.InstanceCount();
        InstanceChunk chunk{};
        uint32_t chunksStarted = 0;
        gpu::InstanceData instance{};
        for (const InstanceRange &range : ranges) {
            if (range.chunk == chunksStarted) {
                const auto elementCount = static_cast<uint32_t>(
                    std::min<uint64_t>(chunkCapacity, instanceCount - uint64_t{range.chunk} * chunkCapacity));
                chunk = beginChunk(elementCount);
                ++chunksStarted;
            }

            const BatchBucket &bucket = buckets[range.bucket];
            draws.push_back({.key = bucket.key,
                             .instanceBufferIndex = chunk.instanceBufferIndex,
                             .firstElement = range.firstElement,
                             .count = range.count});

            uint64_t offset = uint64_t{range.firstElement} * kInstanceStride;
            for (uint32_t i = 0; i < range.count; ++i) {
                const DirectX::XMFLOAT4X4 &world = bucket.worlds[range.firstObject + i];
                DirectX::XMStoreFloat4x4(&instance.world, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&world)));
                chunk.memory.Write(offset, instance);
                offset += kInstanceStride;
            }
        }
    }
} // namespace render

#endif // D3D12_ENGINE_INSTANCEUPLOAD_H
