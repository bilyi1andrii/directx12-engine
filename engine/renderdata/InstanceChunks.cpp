#include "renderdata/InstanceChunks.h"

#include <algorithm>

namespace render {
    void SplitIntoChunks(std::span<const BatchBucket> buckets, uint32_t chunkCapacity,
                         std::vector<InstanceRange> &ranges) {
        ranges.clear();

        uint32_t chunk = 0;
        uint32_t element = 0;
        for (uint32_t bucketIndex = 0; bucketIndex < buckets.size(); ++bucketIndex) {
            const auto objectCount = static_cast<uint32_t>(buckets[bucketIndex].worlds.size());
            uint32_t firstObject = 0;
            while (firstObject < objectCount) {
                if (element == chunkCapacity) {
                    ++chunk;
                    element = 0;
                }
                const uint32_t count = std::min(objectCount - firstObject, chunkCapacity - element);
                ranges.push_back({.bucket = bucketIndex,
                                  .firstObject = firstObject,
                                  .chunk = chunk,
                                  .firstElement = element,
                                  .count = count});
                firstObject += count;
                element += count;
            }
        }
    }
} // namespace render
