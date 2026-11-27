#include "BenchReport.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string_view>
#include <vector>

#include "bench/BenchOutput.h"
#include "common/AppChecks.h"
#include "common/GridSetup.h"
#include "core/Log.h"
#include "core/Utf8.h"
#include "renderdata/RendererSettings.h"

namespace fs = std::filesystem;
namespace chrono = std::chrono;

using core::LogError;
using core::LogInfo;

namespace bench {
    namespace {
#if defined(__SANITIZE_ADDRESS__)
        constexpr std::string_view kBuildConfig = ENGINE_BUILD_CONFIG " (ASan)";
#else
        constexpr std::string_view kBuildConfig = ENGINE_BUILD_CONFIG;
#endif

        std::vector<RunField> RunInfo(const BenchOptions &options, const BenchRun &run, std::string_view started,
                                      double medianFrameMs, double medianRecordMs, double medianGpuMs) {
            const bool instanced = options.drawPath == render::DrawPath::Instanced;
            std::vector<RunField> fields{
                {"engine", std::string(kEngineName)},
                {"scene", core::ToUtf8(BenchSceneName(options).native())},
                {"grid", std::string(apps::ToString(options.gridMode))},
                {"objects", std::format("{} (--count {})", run.objectCount, options.count)},
                {"model", options.modelPath ? core::ToUtf8(options.modelPath->native()) : "cube"},
                {"parts", std::format("{}", run.partCount)},
                {"instances", std::format("{}", run.objectCount * run.partCount)},
                {"transparent_parts", std::format("{}", run.transparentPartCount)},
                {"transparent_draws", std::format("{}", run.objectCount * run.transparentDrawsPerObject)},
                {"triangles", std::format("{}", run.triangleCount)},
                {"lights", run.lights},
                {"draw_path", instanced ? "instanced" : "non-instanced"},
                {"resolution", std::format("{}x{}", run.width, run.height)},
                {"window", "borderless fullscreen"},
                {"vsync", "off"},
                {"tearing", run.tearingSupported ? "supported" : "not supported"},
                {"warmup_frames", std::format("{}", options.warmupFrames)},
                {"measured_frames", std::format("{}", options.frames)},
                {"build", std::string(kBuildConfig)},
                {"gpu", run.adapterName},
                {"driver", run.driverVersion},
                {"started", std::string(started)},
                {"median_cpu_frame_ms", std::format("{:.4f}", medianFrameMs)},
                {"median_cpu_record_ms", std::format("{:.4f}", medianRecordMs)},
                {"median_gpu_total_ms", std::format("{:.4f}", medianGpuMs)},
            };
            for (size_t pass = 0; pass < run.gpuPasses.size(); ++pass) {
                const double median = Median(run.timings, [pass](const FrameTiming &t) { return t.gpuPassMs[pass]; });
                fields.push_back({"median_" + GpuPassColumn(run.gpuPasses[pass]), std::format("{:.4f}", median)});
            }
            return fields;
        }
    } // namespace

    int ReportRun(const BenchOptions &options, const std::expected<BenchRun, std::string> &outcome,
                  chrono::system_clock::time_point started, [[maybe_unused]] const rhi::ValidationStats &validation) {
        if (!outcome) {
            LogError("{}; nothing written", outcome.error());
            return kExitBenchFailed;
        }
        const BenchRun &run = *outcome;

        const uint64_t expectedFrames = uint64_t{options.warmupFrames} + options.frames;
        if (run.result.framesRendered != expectedFrames) {
            LogError("Bench stopped early: {} of {} frames rendered; nothing written", run.result.framesRendered,
                     expectedFrames);
            return kExitBenchFailed;
        }

        const double medianFrameMs = Median(run.timings, [](const FrameTiming &t) { return t.cpuFrameMs; });
        const double medianRecordMs = Median(run.timings, [](const FrameTiming &t) { return t.cpuRecordMs; });
        const double medianGpuMs = Median(run.timings, [](const FrameTiming &t) { return t.gpuTotalMs; });
        LogInfo("Measured {} frames: medians cpu_frame_ms {:.3f}, cpu_record_ms {:.3f}, gpu_total_ms {:.3f}",
                run.timings.size(), medianFrameMs, medianRecordMs, medianGpuMs);

        const std::string timestamp = FolderTimestamp(started);
        const std::vector<RunField> fields =
            RunInfo(options, run, timestamp, medianFrameMs, medianRecordMs, medianGpuMs);
        const std::expected<fs::path, std::string> folder =
            WriteResults(options.outDir, BenchSceneName(options), timestamp, run.gpuPasses, run.timings, fields);
        if (!folder) {
            LogError("Results not written: {}", folder.error());
            return kExitBenchFailed;
        }
        LogInfo("Results: {}", core::ToUtf8(folder->native()));

#if defined(_DEBUG)
        return apps::SmokeTestExitCode(expectedFrames, run.result, validation);
#else
        return run.result.exitCode;
#endif
    }
} // namespace bench
