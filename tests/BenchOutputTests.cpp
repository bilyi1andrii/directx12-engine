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
    const std::array<std::string, 2> passes{"Opaque", "UI"};
    constexpr std::array timings{
        FrameTiming{.cpuFrameMs = 0.8346, .cpuRecordMs = 0.01749, .gpuTotalMs = 0.462, .gpuPassMs = {0.456, 0.00551}},
        FrameTiming{.cpuFrameMs = 12.0, .cpuRecordMs = 3.5, .gpuTotalMs = 2.0, .gpuPassMs = {1.75, 0.25}},
    };
    std::ostringstream out;
    engine::bench::WriteFramesCsv(out, passes, timings);
    EXPECT_EQ(out.str(), "frame,cpu_frame_ms,cpu_record_ms,gpu_total_ms,gpu_opaque_ms,gpu_ui_ms\n"
                         "0,0.8346,0.0175,0.4620,0.4560,0.0055\n"
                         "1,12.0000,3.5000,2.0000,1.7500,0.2500\n");
}

TEST(BenchOutput, GpuPassColumnIsOneLowerCaseWord) {
    EXPECT_EQ(engine::bench::GpuPassColumn("Opaque"), "gpu_opaque_ms");
    EXPECT_EQ(engine::bench::GpuPassColumn("Shadow Map 2"), "gpu_shadow_map_2_ms");
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

    const std::array<std::string, 1> passes{"Opaque"};
    constexpr std::array timings{FrameTiming{.cpuFrameMs = 1.0, .cpuRecordMs = 0.5}};
    const std::array fields{RunField{.key = "scene", .value = "stress"}};
    // Two runs that started in the same second
    const auto first = engine::bench::WriteResults(results.path, "stress", "20261107-221530", passes, timings, fields);
    const auto second = engine::bench::WriteResults(results.path, "stress", "20261107-221530", passes, timings, fields);
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
