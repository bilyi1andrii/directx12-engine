#ifndef D3D12_ENGINE_INSTANCECHUNKS_H
#define D3D12_ENGINE_INSTANCECHUNKS_H

#include <cstdint>
#include <span>
#include <vector>

#include "renderdata/RenderList.h"

namespace render {
    // Consecutive objects of one bucket that lie in one chunk: one instanced draw.
    //
    // The renderer writes instance data in chunks of at most `chunkCapacity` objects, because a shader view
    // covers one buffer and every upload-ring page is its own buffer. Objects are numbered across the buckets
    // in order; object g goes to chunk g / chunkCapacity. A bucket that crosses a chunk boundary gives two ranges.
    struct InstanceRange {
        uint32_t bucket = 0;       // index into RenderList::Buckets()
        uint32_t firstObject = 0;  // index into the bucket's `worlds`
        uint32_t chunk = 0;        // the chunk that holds the range
        uint32_t firstElement = 0; // position of the first object in that chunk
        uint32_t count = 0;        // objects in the range, >= 1
    };

    // One instanced draw: `count` objects with the same key, from element `firstElement` of the instance
    // buffer at bindless index `instanceBufferIndex`. Static and dynamic objects differ only in that buffer.
    struct InstanceDraw {
        BatchKey key;
        uint32_t instanceBufferIndex = 0;
        uint32_t firstElement = 0;
        uint32_t count = 0;
    };

    // Fills `ranges` with the ranges of `buckets`, in order; empty buckets give none. `ranges` is passed in so its
    // memory is reused every frame.
    void SplitIntoChunks(std::span<const BatchBucket> buckets, uint32_t chunkCapacity,
                         std::vector<InstanceRange> &ranges);
} // namespace render

#endif // D3D12_ENGINE_INSTANCECHUNKS_H
