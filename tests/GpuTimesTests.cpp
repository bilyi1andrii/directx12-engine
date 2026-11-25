// Tests turning GPU timestamps into frame and pass times, and keeping pass names.

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "rhi/GpuTimes.h"

using rhi::GpuFrameTimes;
using rhi::GpuScopeNames;
using rhi::kMaxGpuScopes;
using rhi::MakeGpuFrameTimes;
using rhi::TicksToMs;
namespace timestamp = rhi::timestamp;

TEST(GpuTimes, EveryTimestampOfAFrameHasItsOwnIndex) {
    std::set<uint32_t> indices{timestamp::kFrameBegin, timestamp::kFrameEnd};
    for (uint32_t scope = 0; scope < kMaxGpuScopes; ++scope) {
        indices.insert(timestamp::ScopeBegin(scope));
        indices.insert(timestamp::ScopeEnd(scope));
    }
    EXPECT_EQ(indices.size(), timestamp::kPerFrame);
    EXPECT_EQ(*indices.rbegin(), timestamp::kPerFrame - 1);
    EXPECT_EQ(timestamp::ScopeEnd(1), timestamp::Count(2) - 1);
}

TEST(GpuTimes, TicksToMilliseconds) {
    constexpr uint64_t kTicksPerSecond = 10'000'000;
    EXPECT_DOUBLE_EQ(TicksToMs(1'000, 26'000, kTicksPerSecond), 2.5);
    EXPECT_DOUBLE_EQ(TicksToMs(7, 7, kTicksPerSecond), 0.0);
    constexpr uint64_t kLate = 0xF000'0000'0000'0000;
    EXPECT_DOUBLE_EQ(TicksToMs(kLate, kLate + 10'001, kTicksPerSecond), 1.0001);
}

TEST(GpuTimes, EndBeforeBeginIsZero) {
    EXPECT_DOUBLE_EQ(TicksToMs(5'000, 4'000, 1'000'000), 0.0);
}

TEST(GpuTimes, FrameTimesFromTicks) {
    constexpr uint64_t kTicksPerSecond = 1'000'000;
    std::array<uint64_t, timestamp::Count(2)> ticks{};
    ticks[timestamp::kFrameBegin] = 1'000;
    ticks[timestamp::kFrameEnd] = 21'000;
    ticks[timestamp::ScopeBegin(0)] = 2'000;
    ticks[timestamp::ScopeEnd(0)] = 12'000;
    ticks[timestamp::ScopeBegin(1)] = 12'500;
    ticks[timestamp::ScopeEnd(1)] = 20'000;
    const std::array<std::string_view, 2> names{"Opaque", "UI"};

    const GpuFrameTimes times = MakeGpuFrameTimes(42, ticks, names, kTicksPerSecond);

    EXPECT_EQ(times.frame, 42u);
    EXPECT_DOUBLE_EQ(times.totalMs, 20.0);
    ASSERT_EQ(times.Scopes().size(), 2u);
    EXPECT_EQ(times.Scopes()[0].name, "Opaque");
    EXPECT_DOUBLE_EQ(times.Scopes()[0].ms, 10.0);
    EXPECT_EQ(times.Scopes()[1].name, "UI");
    EXPECT_DOUBLE_EQ(times.Scopes()[1].ms, 7.5);
}

TEST(GpuTimes, ScopeNamesAreStoredOnce) {
    GpuScopeNames names;
    const uint32_t opaque = names.Register("Opaque");
    const uint32_t ui = names.Register("UI");
    EXPECT_NE(opaque, ui);
    EXPECT_EQ(names.Register(std::string("Opaque")), opaque);
    EXPECT_EQ(names.Name(opaque), "Opaque");
    EXPECT_EQ(names.Name(ui), "UI");
}

TEST(GpuTimes, ScopeNameViewsSurviveNewNames) {
    GpuScopeNames names;
    const std::string_view opaque = names.Name(names.Register("Opaque"));
    for (int i = 0; i < 1'000; ++i) {
        (void)names.Register("Pass" + std::to_string(i));
    }
    EXPECT_EQ(opaque, "Opaque");
    EXPECT_EQ(opaque.data(), names.Name(names.Register("Opaque")).data());
}
