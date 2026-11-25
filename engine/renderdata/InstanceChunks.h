#ifndef D3D12_ENGINE_INSTANCECHUNKS_H
#define D3D12_ENGINE_INSTANCECHUNKS_H

// Splits a frame's objects into chunks that each fit one upload page, 
// so every instanced draw reads from a single buffer.

#include <cstdint>
#include <span>
#include <vector>

#include "renderdata/RenderList.h"

namespace render {
    struct InstanceRange {
        uint32_t bucket = 0;
        uint32_t firstObject = 0;
        uint32_t chunk = 0;
        uint32_t firstElement = 0;
        uint32_t count = 0;
    };

    struct InstanceDraw {
        BatchKey key;
        uint32_t instanceBufferIndex = 0; // bindless
        uint32_t firstElement = 0;
        uint32_t count = 0;
    };

    void SplitIntoChunks(std::span<const BatchBucket> buckets, uint32_t chunkCapacity,
                         std::vector<InstanceRange> &ranges);
} // namespace render

#endif // D3D12_ENGINE_INSTANCECHUNKS_H
