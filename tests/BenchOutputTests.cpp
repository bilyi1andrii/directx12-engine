#include <array>
#include <expected>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "bench/BenchOutput.h"

using engine::bench::FrameTiming;
using engine::bench::RunField;

TEST(BenchOutput, FramesCsvHasTheHeaderAndOneRowPerFrame) {
    constexpr std::array timings{
        FrameTiming{.cpuFrameMs = 0.8346, .cpuRecordMs = 0.01749},
        FrameTiming{.cpuFrameMs = 12.0, .cpuRecordMs = 3.5},
    };
    std::ostringstream out;
    engine::bench::WriteFramesCsv(out, timings);
    EXPECT_EQ(out.str(), "frame,cpu_frame_ms,cpu_record_ms\n"
                         "0,0.8346,0.0175\n"
                         "1,12.0000,3.5000\n");
}

TEST(BenchOutput, ResultsNeverOverwriteAnEarlierRun) {
    // Removed at the end of the test, also when an expectation fails
    struct TempFolder {
        std::filesystem::path path = std::filesystem::temp_directory_path() / "engine_tests_bench_output";
        ~TempFolder() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    } results;
    std::error_code ignored;
    std::filesystem::remove_all(results.path, ignored); // left over from an aborted run

    constexpr std::array timings{FrameTiming{.cpuFrameMs = 1.0, .cpuRecordMs = 0.5}};
    const std::array fields{RunField{.key = "scene", .value = "stress"}};
    // Two runs that started in the same second
    const auto first = engine::bench::WriteResults(results.path, "stress", "20261107-221530", timings, fields);
    const auto second = engine::bench::WriteResults(results.path, "stress", "20261107-221530", timings, fields);
    ASSERT_TRUE(first.has_value()) << first.error();
    ASSERT_TRUE(second.has_value()) << second.error();

    const std::filesystem::path expected = results.path / "dx12" / "stress" / "20261107-221530";
    EXPECT_EQ(*first, expected);
    EXPECT_EQ(second->filename(), "20261107-221530-2");
    for (const std::filesystem::path &folder : {*first, *second}) {
        EXPECT_TRUE(std::filesystem::is_regular_file(folder / "frames.csv")) << folder;
        EXPECT_TRUE(std::filesystem::is_regular_file(folder / "run.txt")) << folder;
    }
}
