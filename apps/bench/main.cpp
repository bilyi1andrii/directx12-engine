// The benchmark: flies a scripted camera over a grid of copies of a model and writes the time of every frame to
// results/dx12/<scene>/<timestamp>/. It runs unattended, so it never shows a dialog.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "BenchOptions.h"
#include "BenchOutput.h"
#include "StressGrid.h"
#include "assets/AssetCache.h"
#include "common/AppChecks.h"
#include "common/AppMain.h"
#include "common/AppShell.h"
#include "common/GridSetup.h"
#include "core/Input.h"
#include "core/Log.h"
#include "core/Utf8.h"
#include "core/Window.h"
#include "rhi/AgilitySdk.h"
#include "rhi/GpuTimes.h"
#include "rhi/ValidationStats.h"
#include "render/Renderer.h"
#include "renderdata/LightPacking.h"
#include "scene/Camera.h"
#include "scene/CameraPath.h"

namespace fs = std::filesystem;
namespace chrono = std::chrono;

using bench::FrameTiming;
using bench::RunField;
using core::LogError;
using core::LogInfo;
using core::LogWarning;

ENGINE_EXPORT_AGILITY_SDK()

namespace {
    using Clock = chrono::steady_clock;

    constexpr int kExitBenchFailed = 1;

#if defined(__SANITIZE_ADDRESS__)
    constexpr std::string_view kBuildConfig = ENGINE_BUILD_CONFIG " (ASan)";
#else
    constexpr std::string_view kBuildConfig = ENGINE_BUILD_CONFIG;
#endif

    double ToMs(Clock::duration duration) {
        return chrono::duration<double, std::milli>(duration).count();
    }

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

    static_assert(bench::kMaxGpuPasses == rhi::kMaxGpuScopes);

    struct BenchRun {
        apps::RunResult result;
        std::vector<FrameTiming> timings;
        std::vector<std::string> gpuPasses;
        std::string adapterName;
        std::string driverVersion;
        uint32_t width = 0;
        uint32_t height = 0;
        bool tearingSupported = false;
        size_t objectCount = 0;
        size_t partCount = 0;
        size_t transparentPartCount = 0;
        size_t transparentDrawsPerObject = 0;
        uint64_t triangleCount = 0;
        std::string lights;
    };

    std::string DescribeLights(const render::LightSet &lights) {
        const render::PackedLightCounts counts = render::CountPackedLights(lights);
        return std::format("{} directional, {} point, {} spot", counts.directional, counts.points, counts.spots);
    }

    std::expected<const render::Model *, std::string> LoadGridModel(const BenchOptions &options,
                                                                    assets::AssetCache &assets) {
        if (!options.modelPath) {
            return &assets.CubeModel(assets.CheckerboardTexture());
        }
        const auto model = assets.LoadModel(*options.modelPath);
        if (!model) {
            return std::unexpected(model.error().message);
        }
        return *model;
    }

    std::expected<BenchRun, std::string> RunBench(const BenchOptions &options, rhi::ValidationStats &validation) {
        apps::AppShell shell({.title = L"D3D12 Engine - bench", .fullscreen = true}, validation);
        core::Input &input = shell.Input();
        core::Window &window = shell.Window();
        render::Renderer &renderer = shell.Renderer();
        if (!renderer.TearingSupported()) {
            LogWarning("Tearing not supported: Present without vsync may still wait for the display");
        }

        assets::AssetCache assets(renderer.Resources());
        const std::expected<const render::Model *, std::string> model = LoadGridModel(options, assets);
        if (!model) {
            return std::unexpected(model.error());
        }

        render::RenderList renderList;
        const bench::StressGrid grid(renderer.Resources(), options.gridMode, options.count, **model);
        const std::array path = grid.CameraPath();
        scene::Camera camera;
        camera.fovDegrees = 60.0f;
        camera.farZ = std::max(camera.farZ, 2.0f * grid.Extent() + 100.0f);

        const render::RendererSettings settings{.drawPath = options.drawPath, .vsync = false};
        const render::LightSet lights = grid.Lights();

        BenchRun run{.adapterName = renderer.AdapterName(),
                     .driverVersion = renderer.DriverVersion(),
                     .width = window.Width(),
                     .height = window.Height(),
                     .tearingSupported = renderer.TearingSupported(),
                     .objectCount = grid.ObjectCount(),
                     .partCount = (*model)->parts.size(),
                     .triangleCount = grid.ObjectCount() * (*model)->triangleCount,
                     .lights = DescribeLights(lights)};
        for (const render::ModelPart &part : (*model)->parts) {
            const render::MaterialInfo &material = renderer.Resources().Material(part.material);
            if (material.alphaMode == render::AlphaMode::Blend) {
                ++run.transparentPartCount;
                run.transparentDrawsPerObject += material.doubleSided ? 2 : 1;
            }
        }
        LogInfo("Bench: {}x{}, {} warm-up + {} measured frames", run.width, run.height, options.warmupFrames,
                options.frames);

        run.timings.resize(options.frames);
        const uint64_t totalFrames = uint64_t{options.warmupFrames} + options.frames;
        uint64_t frame = 0;

        uint64_t nextGpuFrame = options.warmupFrames;
        std::optional<std::string> gpuError;
        const auto takeGpuTimes = [&](const rhi::GpuFrameTimes &times) {
            if (gpuError || times.frame < nextGpuFrame || times.frame >= totalFrames) {
                return;
            }
            if (times.frame != nextGpuFrame) {
                gpuError = std::format("no GPU times for frame {}", nextGpuFrame);
                return;
            }
            const std::span<const rhi::GpuScopeTime> passes = times.Scopes();
            if (nextGpuFrame == options.warmupFrames) {
                for (const rhi::GpuScopeTime &pass : passes) {
                    run.gpuPasses.emplace_back(pass.name);
                }
            } else if (!std::ranges::equal(passes, run.gpuPasses, {}, &rhi::GpuScopeTime::name)) {
                gpuError = std::format("the frame graph's passes changed at frame {}", times.frame);
                return;
            }
            FrameTiming &timing = run.timings[times.frame - options.warmupFrames];
            timing.gpuTotalMs = times.totalMs;
            for (size_t pass = 0; pass < passes.size(); ++pass) {
                timing.gpuPassMs[pass] = passes[pass].ms;
            }
            ++nextGpuFrame;
        };

        Clock::time_point frameStart{};
        while (true) {
            const Clock::time_point now = Clock::now();
            if (frame > options.warmupFrames) {
                run.timings[frame - 1 - options.warmupFrames].cpuFrameMs = ToMs(now - frameStart);
            }
            frameStart = now;
            if (frame == totalFrames || !window.PumpMessages()) {
                break;
            }
            if (input.WasKeyPressed(VK_ESCAPE)) {
                window.Close();
            }
            input.EndFrame();

            const uint32_t pathFrame =
                frame < options.warmupFrames ? 0 : static_cast<uint32_t>(frame - options.warmupFrames);
            const scene::CameraPose pose = scene::EvaluateCameraPath(path, pathFrame);
            camera.position = pose.position;
            scene::LookAt(camera, pose.target);

            renderer.BeginFrame(window.Width(), window.Height(), settings);
            if (const std::optional<rhi::GpuFrameTimes> &gpu = renderer.GpuTimes()) {
                takeGpuTimes(*gpu);
            }
            const Clock::time_point recordStart = Clock::now();
            renderList.Clear();
            grid.Submit(renderList);
            renderer.Render(scene::MakeFrameView(camera, renderer.AspectRatio()), lights, renderList,
                            grid.StaticLists());
            const Clock::time_point recordEnd = Clock::now();
            if (frame >= options.warmupFrames) {
                run.timings[frame - options.warmupFrames].cpuRecordMs = ToMs(recordEnd - recordStart);
            }
            renderer.EndFrame();
            ++frame;
        }

        for (const rhi::GpuFrameTimes &times : renderer.FinishGpuTimes()) {
            takeGpuTimes(times);
        }
        run.result = {.exitCode = window.ExitCode(), .framesRendered = frame};
        if (frame == totalFrames) {
            if (!gpuError && nextGpuFrame != totalFrames) {
                gpuError = std::format("no GPU times for frame {}", nextGpuFrame);
            }
            if (gpuError) {
                return std::unexpected("GPU times: " + *gpuError);
            }
        }
        return run;
    }

    std::vector<RunField> RunInfo(const BenchOptions &options, const BenchRun &run, std::string_view started,
                                  double medianFrameMs, double medianRecordMs, double medianGpuMs) {
        const bool instanced = options.drawPath == render::DrawPath::Instanced;
        std::vector<RunField> fields{
            {"engine", std::string(bench::kEngineName)},
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
            fields.push_back({"median_" + bench::GpuPassColumn(run.gpuPasses[pass]), std::format("{:.4f}", median)});
        }
        return fields;
    }

    int FinishBench(const BenchOptions &options, const std::expected<BenchRun, std::string> &outcome,
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

        const std::string timestamp = bench::FolderTimestamp(started);
        const std::vector<RunField> fields =
            RunInfo(options, run, timestamp, medianFrameMs, medianRecordMs, medianGpuMs);
        const std::expected<fs::path, std::string> folder =
            bench::WriteResults(options.outDir, BenchSceneName(options), timestamp, run.gpuPasses, run.timings, fields);
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
} // namespace

int WINAPI wWinMain(HINSTANCE /*hInst*/, HINSTANCE /*prevInst*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/) {
    std::optional<BenchOptions> options;
    chrono::system_clock::time_point started;
    std::optional<std::expected<BenchRun, std::string>> outcome;
    return apps::AppMain({
        .usage = kBenchUsage,
        .errorDialog = false,
        .parse = [&options](std::span<const std::string> args) -> std::expected<apps::RunKind, std::string> {
            std::expected<BenchOptions, std::string> parsed = ParseBenchOptions(args);
            if (!parsed) {
                return std::unexpected(std::move(parsed.error()));
            }
            options = std::move(*parsed);
            return apps::RunKind::Unattended;
        },
        .run =
            [&](rhi::ValidationStats &validation) {
                LogInfo("Bench: {}, grid {}, --count {}, {}, results in '{}'",
                        core::ToUtf8(BenchSceneName(*options).native()), apps::ToString(options->gridMode),
                        options->count,
                        options->drawPath == render::DrawPath::Instanced ? "instanced" : "non-instanced",
                        core::ToUtf8(options->outDir.native()));
                started = chrono::system_clock::now();
                outcome = RunBench(*options, validation);
            },
        .finish =
            [&](const rhi::ValidationStats &validation) {
                return FinishBench(*options, *outcome, started, validation);
            },
    });
}
