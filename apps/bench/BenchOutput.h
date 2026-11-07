#ifndef D3D12_ENGINE_BENCHOUTPUT_H
#define D3D12_ENGINE_BENCHOUTPUT_H

#include <chrono>
#include <expected>
#include <filesystem>
#include <ostream>
#include <span>
#include <string>
#include <string_view>

// What a benchmark run writes (CPU only, so engine_tests can test it): results/<engine>/<scene>/<timestamp>/
// with frames.csv (one row per measured frame) and run.txt (the settings and the machine)
namespace engine::bench {
    // One measured frame, in milliseconds (exact definitions: .claude/rules/bench-and-tools.md)
    struct FrameTiming {
        // From the start of this frame to the start of the next: everything, including the fence wait in
        // BeginFrame, Present and the message pump
        double cpuFrameMs = 0.0;
        // Building this frame's dynamic list + Renderer::Render: extraction, instance upload, command recording
        double cpuRecordMs = 0.0;
    };

    // One line of run.txt: "key: value"
    struct RunField {
        std::string key;
        std::string value;
    };

    // The engine's folder under the results root; DX11 writes to results/dx11/
    inline constexpr std::string_view kEngineName = "dx12";

    // frames.csv: header `frame,cpu_frame_ms,cpu_record_ms`, then one row per frame, numbered from 0.
    // New columns only at the end (the DX11 harness and the tools/ scripts read the same schema).
    void WriteFramesCsv(std::ostream &out, std::span<const FrameTiming> timings);

    // run.txt: one "key: value" line per field
    void WriteRunInfo(std::ostream &out, std::span<const RunField> fields);

    // The folder name of a run started at `time`: local time as YYYYMMDD-HHMMSS
    [[nodiscard]] std::string FolderTimestamp(std::chrono::system_clock::time_point time);

    // Creates <outDir>/dx12/<scene>/<timestamp>/ and writes frames.csv and run.txt into it. Never overwrites a
    // run: if that folder exists (two runs started in the same second), the name gets "-2", "-3", ...
    // Returns the folder, or why it could not be written.
    [[nodiscard]] std::expected<std::filesystem::path, std::string>
    WriteResults(const std::filesystem::path &outDir, std::string_view scene, std::string_view timestamp,
                 std::span<const FrameTiming> timings, std::span<const RunField> fields);
} // namespace engine::bench

#endif // D3D12_ENGINE_BENCHOUTPUT_H
