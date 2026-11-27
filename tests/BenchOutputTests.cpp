// Tests the bench's result files: the CSV layout, the column names and never overwriting a run.

#include <array>
#include <expected>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "bench/BenchOutput.h"

namespace fs = std::filesystem;

using bench::FrameTiming;
using bench::RunField;

TEST(BenchOutput, FramesCsvHasTheHeaderAndOneRowPerFrame) {
    const std::array<std::string, 2> passes{"Opaque", "UI"};
    constexpr std::array timings{
        FrameTiming{.cpuFrameMs = 0.8346, .cpuRecordMs = 0.01749, .gpuTotalMs = 0.462, .gpuPassMs = {0.456, 0.00551}},
        FrameTiming{.cpuFrameMs = 12.0, .cpuRecordMs = 3.5, .gpuTotalMs = 2.0, .gpuPassMs = {1.75, 0.25}},
    };
    std::ostringstream out;
    bench::WriteFramesCsv(out, passes, timings);
    EXPECT_EQ(out.str(), "frame,cpu_frame_ms,cpu_record_ms,gpu_total_ms,gpu_opaque_ms,gpu_ui_ms\n"
                         "0,0.8346,0.0175,0.4620,0.4560,0.0055\n"
                         "1,12.0000,3.5000,2.0000,1.7500,0.2500\n");
}

TEST(BenchOutput, GpuPassColumnIsOneLowerCaseWord) {
    EXPECT_EQ(bench::GpuPassColumn("Opaque"), "gpu_opaque_ms");
    EXPECT_EQ(bench::GpuPassColumn("Shadow Map 2"), "gpu_shadow_map_2_ms");
}

TEST(BenchOutput, ResultsNeverOverwriteAnEarlierRun) {
    struct TempFolder {
        fs::path path = fs::temp_directory_path() / "engine_tests_bench_output";
        ~TempFolder() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } results;
    std::error_code ignored;
    fs::remove_all(results.path, ignored);

    const std::array<std::string, 1> passes{"Opaque"};
    constexpr std::array timings{FrameTiming{.cpuFrameMs = 1.0, .cpuRecordMs = 0.5}};
    const std::array fields{RunField{.key = "scene", .value = "stress"}};
    const auto first = bench::WriteResults(results.path, "stress", "20261107-221530", passes, timings, fields);
    const auto second = bench::WriteResults(results.path, "stress", "20261107-221530", passes, timings, fields);
    ASSERT_TRUE(first.has_value()) << first.error();
    ASSERT_TRUE(second.has_value()) << second.error();

    const fs::path expected = results.path / "dx12" / "stress" / "20261107-221530";
    EXPECT_EQ(*first, expected);
    EXPECT_EQ(second->filename(), "20261107-221530-2");
    for (const fs::path &folder : {*first, *second}) {
        EXPECT_TRUE(fs::is_regular_file(folder / "frames.csv")) << folder;
        EXPECT_TRUE(fs::is_regular_file(folder / "run.txt")) << folder;
    }
}
