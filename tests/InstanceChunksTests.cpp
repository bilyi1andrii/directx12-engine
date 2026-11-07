#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include <DirectXMath.h>
#include <gtest/gtest.h>

#include "renderdata/InstanceChunks.h"
#include "renderdata/RenderList.h"

using engine::render::BatchBucket;
using engine::render::InstanceRange;
using engine::render::SplitIntoChunks;

namespace {
    // Buckets with the given object counts; only the counts matter for the split
    [[nodiscard]] std::vector<BatchBucket> BucketsOfSize(std::initializer_list<size_t> counts) {
        std::vector<BatchBucket> buckets;
        for (const size_t count : counts) {
            buckets.push_back({.key = {}, .worlds = std::vector<DirectX::XMFLOAT4X4>(count)});
        }
        return buckets;
    }

    void ExpectRange(const InstanceRange &range, uint32_t bucket, uint32_t firstObject, uint32_t chunk,
                     uint32_t firstElement, uint32_t count) {
        EXPECT_EQ(range.bucket, bucket);
        EXPECT_EQ(range.firstObject, firstObject);
        EXPECT_EQ(range.chunk, chunk);
        EXPECT_EQ(range.firstElement, firstElement);
        EXPECT_EQ(range.count, count);
    }
} // namespace

TEST(InstanceChunks, BucketCrossingAChunkBoundaryIsSplit) {
    // Capacity 4: bucket 0 fills elements 0-2 of chunk 0, bucket 1 continues at element 3 and
    // spills 3 objects into chunk 1
    const std::vector<BatchBucket> buckets = BucketsOfSize({3, 4});
    std::vector<InstanceRange> ranges;

    SplitIntoChunks(buckets, 4, ranges);

    ASSERT_EQ(ranges.size(), 3u);
    ExpectRange(ranges[0], 0, 0, 0, 0, 3);
    ExpectRange(ranges[1], 1, 0, 0, 3, 1);
    ExpectRange(ranges[2], 1, 1, 1, 0, 3);
}

TEST(InstanceChunks, BucketLargerThanTwoChunksAndEmptyBuckets) {
    // An empty bucket gives no range; a full chunk is followed by a new one, not an empty range
    const std::vector<BatchBucket> buckets = BucketsOfSize({0, 9, 0, 1});
    std::vector<InstanceRange> ranges{InstanceRange{}}; // old contents are replaced

    SplitIntoChunks(buckets, 4, ranges);

    ASSERT_EQ(ranges.size(), 4u);
    ExpectRange(ranges[0], 1, 0, 0, 0, 4);
    ExpectRange(ranges[1], 1, 4, 1, 0, 4);
    ExpectRange(ranges[2], 1, 8, 2, 0, 1);
    ExpectRange(ranges[3], 3, 0, 2, 1, 1);
}
