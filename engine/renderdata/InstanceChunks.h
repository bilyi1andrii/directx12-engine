#ifndef D3D12_ENGINE_INSTANCECHUNKS_H
#define D3D12_ENGINE_INSTANCECHUNKS_H

#include <cstdint>
#include <span>
#include <vector>

#include "renderdata/RenderList.h"

namespace engine::render {
    // A run of one bucket's objects that lies in one chunk: one instanced draw, or `count` single draws.
    //
    // The renderer writes the frame's instance data as chunks of at most `chunkCapacity` elements, because
    // one shader view (SRV) covers a range of one buffer, and the upload ring's pages are separate buffers.
    // Objects are numbered across all buckets in bucket order; object g goes to chunk g / chunkCapacity,
    // element g % chunkCapacity. A bucket that crosses a chunk boundary is split into two ranges.
    struct InstanceRange {
        uint32_t bucket = 0;       // index into RenderList::Buckets()
        uint32_t firstObject = 0;  // first object of the bucket in this range (index into its `worlds`)
        uint32_t chunk = 0;        // the chunk that holds the range
        uint32_t firstElement = 0; // element of the first object in that chunk
        uint32_t count = 0;        // objects in the range, >= 1
    };

    // Replaces the contents of `ranges` with the ranges of `buckets`, in bucket and object order.
    // Empty buckets produce no range. `ranges` is an output parameter so its capacity is reused every frame.
    void SplitIntoChunks(std::span<const BatchBucket> buckets, uint32_t chunkCapacity,
                         std::vector<InstanceRange> &ranges);
} // namespace engine::render

#endif // D3D12_ENGINE_INSTANCECHUNKS_H
