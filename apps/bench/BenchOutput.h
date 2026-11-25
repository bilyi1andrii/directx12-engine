#ifndef D3D12_ENGINE_BENCHOUTPUT_H
#define D3D12_ENGINE_BENCHOUTPUT_H

#include <array>
#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <ostream>
#include <span>
#include <string>
#include <string_view>

// The files a benchmark run writes. CPU only, so the unit tests can check them.
namespace engine::bench {
    inline constexpr size_t kMaxGpuPasses = 16;

    // One measured frame, in milliseconds
    struct FrameTiming {
        // From this frame's start to the next frame's start: everything, including the GPU wait and Present
        double cpuFrameMs = 0.0;
        // Building the frame's list of moving objects + Renderer::Render (upload and command recording)
        double cpuRecordMs = 0.0;
        double gpuTotalMs = 0.0;
        std::array<double, kMaxGpuPasses> gpuPassMs{};
    };

    // One "key: value" line of run.txt
    struct RunField {
        std::string key;
        std::string value;
    };

    // Results go to results/dx12/; the DX11 engine writes to results/dx11/
    inline constexpr std::string_view kEngineName = "dx12";

    // "Opaque" -> "gpu_opaque_ms"
    [[nodiscard]] std::string GpuPassColumn(std::string_view passName);

    // A header line, then one row per frame. Add new columns only at the end: the DX11 engine and the analysis
    // scripts read the same format.
    void WriteFramesCsv(std::ostream &out, std::span<const std::string> gpuPasses,
                        std::span<const FrameTiming> timings);

    void WriteRunInfo(std::ostream &out, std::span<const RunField> fields);

    // Local time as YYYYMMDD-HHMMSS, the name of a run's folder
    [[nodiscard]] std::string FolderTimestamp(std::chrono::system_clock::time_point time);

    // Writes frames.csv and run.txt into a new folder <outDir>/dx12/<scene>/<timestamp>/. Never overwrites a run:
    // if the folder exists, the new one is called <timestamp>-2 (then -3, ...). Returns the folder or the error.
    [[nodiscard]] std::expected<std::filesystem::path, std::string>
    WriteResults(const std::filesystem::path &outDir, const std::filesystem::path &scene, std::string_view timestamp,
                 std::span<const std::string> gpuPasses, std::span<const FrameTiming> timings,
                 std::span<const RunField> fields);
} // namespace engine::bench

#endif // D3D12_ENGINE_BENCHOUTPUT_H
