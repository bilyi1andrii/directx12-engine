#ifndef D3D12_ENGINE_INSTANCEUPLOAD_H
#define D3D12_ENGINE_INSTANCEUPLOAD_H

// Include only from engine_render's .cpp files: it needs the shader include path (common/shared_types.hlsli),
// which users of engine_render do not have.

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

#include <DirectXMath.h>

#include "renderdata/InstanceChunks.h"
#include "renderdata/RenderList.h"
#include "rhi/UploadRing.h"

#include "common/shared_types.hlsli"

namespace engine::render {
    // Bytes per element of every StructuredBuffer<InstanceData>, static or per frame
    inline constexpr uint32_t kInstanceStride = sizeof(gpu::InstanceData);

    // Where WriteInstances writes one chunk, and the view its draws read
    struct InstanceChunk {
        rhi::UploadAllocation memory;
        uint32_t instanceBufferIndex = 0;
    };

    // Shared by dynamic and static instances: splits `list` into chunks of at most `chunkCapacity` objects,
    // writes their transposed world matrices and appends their draws to `draws`. beginChunk(elementCount)
    // is called once per chunk and returns where to write it. `ranges` is scratch memory, passed in so it can be
    // reused.
    template <typename BeginChunk>
    void WriteInstances(const RenderList &list, uint32_t chunkCapacity, BeginChunk &&beginChunk,
                        std::vector<InstanceRange> &ranges, std::vector<InstanceDraw> &draws) {
        SplitIntoChunks(list.Buckets(), chunkCapacity, ranges);
        // Reserve first: once a chunk holds a bindless slot, nothing below may throw
        draws.reserve(draws.size() + ranges.size());

        const std::span<const BatchBucket> buckets = list.Buckets();
        const uint64_t instanceCount = list.InstanceCount();
        InstanceChunk chunk{};
        uint32_t chunksStarted = 0;
        gpu::InstanceData instance{};
        for (const InstanceRange &range : ranges) {
            if (range.chunk == chunksStarted) {
                // First range of a new chunk: start the whole chunk (all but the last chunk are full)
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

            // Written in order, one element after the other: upload memory is write-combined (see UploadRing)
            uint64_t offset = uint64_t{range.firstElement} * kInstanceStride;
            for (uint32_t i = 0; i < range.count; ++i) {
                const DirectX::XMFLOAT4X4 &world = bucket.worlds[range.firstObject + i];
                DirectX::XMStoreFloat4x4(&instance.world, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&world)));
                chunk.memory.Write(offset, instance);
                offset += kInstanceStride;
            }
        }
    }
} // namespace engine::render

#endif // D3D12_ENGINE_INSTANCEUPLOAD_H
