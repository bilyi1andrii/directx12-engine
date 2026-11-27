// Tests how the bench collects GPU times: warm-up frames and frames past the end are skipped, and a missing frame or
// a changed pass list fails the run.

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "bench/BenchOutput.h"
#include "bench/GpuTimeCollector.h"
#include "rhi/GpuTimes.h"

using bench::FrameTiming;
using bench::GpuTimeCollector;

namespace {
    constexpr std::array<std::string_view, 2> kPasses{"Shadow", "Opaque"};
    constexpr std::array<std::string_view, 3> kOtherPasses{"Shadow", "Opaque", "Transparent"};

    rhi::GpuFrameTimes Times(uint64_t frame, std::span<const std::string_view> passes = kPasses) {
        rhi::GpuFrameTimes times{.frame = frame, .totalMs = 1.0 + static_cast<double>(frame)};
        for (size_t i = 0; i < passes.size(); ++i) {
            times.scopes[i] = {.name = passes[i], .ms = static_cast<double>(frame) + 0.25 * static_cast<double>(i)};
        }
        times.scopeCount = static_cast<uint32_t>(passes.size());
        return times;
    }
} // namespace

TEST(GpuTimeCollector, CollectsEveryMeasuredFrame) {
    std::vector<FrameTiming> timings(3);
    GpuTimeCollector collector(timings, 2);
    for (uint64_t frame = 2; frame < 5; ++frame) {
        collector.Add(Times(frame));
    }

    EXPECT_TRUE(collector.Check().has_value());
    EXPECT_EQ(std::vector<std::string>(collector.Passes().begin(), collector.Passes().end()),
              (std::vector<std::string>{"Shadow", "Opaque"}));
    EXPECT_DOUBLE_EQ(timings[0].gpuTotalMs, 3.0);
    EXPECT_DOUBLE_EQ(timings[2].gpuTotalMs, 5.0);
    EXPECT_DOUBLE_EQ(timings[1].gpuPassMs[0], 3.0);
    EXPECT_DOUBLE_EQ(timings[1].gpuPassMs[1], 3.25);
}

TEST(GpuTimeCollector, SkipsWarmUpFrames) {
    std::vector<FrameTiming> timings(2);
    GpuTimeCollector collector(timings, 2);
    for (uint64_t frame = 0; frame < 4; ++frame) {
        collector.Add(Times(frame));
    }

    EXPECT_TRUE(collector.Check().has_value());
    EXPECT_DOUBLE_EQ(timings[0].gpuTotalMs, 3.0);
    EXPECT_DOUBLE_EQ(timings[1].gpuTotalMs, 4.0);
}

TEST(GpuTimeCollector, IgnoresFramesPastTheEnd) {
    std::vector<FrameTiming> timings(2);
    GpuTimeCollector collector(timings, 0);
    for (uint64_t frame = 0; frame < 5; ++frame) {
        collector.Add(Times(frame));
    }

    EXPECT_TRUE(collector.Check().has_value());
    EXPECT_DOUBLE_EQ(timings[1].gpuTotalMs, 2.0);
}

TEST(GpuTimeCollector, FailsOnAMissingFrame) {
    std::vector<FrameTiming> timings(3);
    GpuTimeCollector collector(timings, 0);
    collector.Add(Times(0));
    collector.Add(Times(2));
    collector.Add(Times(1));

    const std::expected<void, std::string> checked = collector.Check();
    ASSERT_FALSE(checked.has_value());
    EXPECT_EQ(checked.error(), "no GPU times for frame 1");
}

TEST(GpuTimeCollector, FailsWhenThePassesChange) {
    std::vector<FrameTiming> timings(3);
    GpuTimeCollector collector(timings, 0);
    collector.Add(Times(0));
    collector.Add(Times(1, kOtherPasses));
    collector.Add(Times(2));

    const std::expected<void, std::string> checked = collector.Check();
    ASSERT_FALSE(checked.has_value());
    EXPECT_EQ(checked.error(), "the frame graph's passes changed at frame 1");
}

TEST(GpuTimeCollector, FailsWhenTheLastFrameNeverArrives) {
    std::vector<FrameTiming> timings(3);
    GpuTimeCollector collector(timings, 1);
    collector.Add(Times(1));
    collector.Add(Times(2));

    const std::expected<void, std::string> checked = collector.Check();
    ASSERT_FALSE(checked.has_value());
    EXPECT_EQ(checked.error(), "no GPU times for frame 3");
}
