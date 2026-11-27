#ifndef D3D12_ENGINE_BENCHOUTPUT_H
#define D3D12_ENGINE_BENCHOUTPUT_H

// The files a benchmark run writes: frames.csv with every measured frame,
// and run.txt with the settings and the medians.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bench {
    inline constexpr size_t kMaxGpuPasses = 16;

    struct FrameTiming {
        double cpuFrameMs = 0.0;
        double cpuRecordMs = 0.0;
        double gpuTotalMs = 0.0;
        std::array<double, kMaxGpuPasses> gpuPassMs{};
    };

    struct RunField {
        std::string key;
        std::string value;
    };

    inline constexpr std::string_view kEngineName = "dx12";

    template <typename Column> double Median(std::span<const FrameTiming> timings, Column column) {
        std::vector<double> values;
        values.reserve(timings.size());
        for (const FrameTiming &timing : timings) {
            values.push_back(column(timing));
        }
        const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
        std::ranges::nth_element(values, middle);
        return *middle;
    }

    [[nodiscard]] std::string GpuPassColumn(std::string_view passName);

    void WriteFramesCsv(std::ostream &out, std::span<const std::string> gpuPasses,
                        std::span<const FrameTiming> timings);

    void WriteRunInfo(std::ostream &out, std::span<const RunField> fields);

    [[nodiscard]] std::string FolderTimestamp(std::chrono::system_clock::time_point time);

    [[nodiscard]] std::expected<std::filesystem::path, std::string>
    WriteResults(const std::filesystem::path &outDir, const std::filesystem::path &scene, std::string_view timestamp,
                 std::span<const std::string> gpuPasses, std::span<const FrameTiming> timings,
                 std::span<const RunField> fields);
} // namespace bench

#endif // D3D12_ENGINE_BENCHOUTPUT_H
