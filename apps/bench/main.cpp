#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "BenchOptions.h"
#include "BenchOutput.h"
#include "StressGrid.h"
#include "assets/AssetCache.h"
#include "common/AppChecks.h"
#include "common/GridSetup.h"
#include "core/CommandLine.h"
#include "core/ComScope.h"
#include "core/Input.h"
#include "core/Log.h"
#include "core/Utf8.h"
#include "core/Window.h"
#include "rhi/AgilitySdk.h"
#include "rhi/ValidationStats.h"
#include "render/Renderer.h"
#include "renderdata/LightPacking.h"
#include "scene/Camera.h"
#include "scene/CameraPath.h"

using engine::bench::FrameTiming;
using engine::bench::RunField;
using engine::core::LogError;
using engine::core::LogInfo;
using engine::core::LogWarning;

ENGINE_EXPORT_AGILITY_SDK()

namespace {
    using Clock = std::chrono::steady_clock;

    // Model not usable, stopped early (Esc, window closed) or results not written. Same value as a failed smoke test.
    constexpr int kExitBenchFailed = 1;

    // Written into run.txt, so a result from the wrong build is easy to spot
#if defined(__SANITIZE_ADDRESS__)
    constexpr std::string_view kBuildConfig = ENGINE_BUILD_CONFIG " (ASan)";
#else
    constexpr std::string_view kBuildConfig = ENGINE_BUILD_CONFIG;
#endif

    double ToMs(Clock::duration duration) {
        return std::chrono::duration<double, std::milli>(duration).count();
    }

    // Takes a copy, because nth_element reorders it
    double Median(std::vector<double> values) {
        if (values.empty()) {
            return 0.0;
        }
        const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
        std::ranges::nth_element(values, middle);
        return *middle;
    }

    struct BenchRun {
        engine::apps::RunResult result;
        std::vector<FrameTiming> timings; // one per measured frame
        // For run.txt: the window and the renderer are gone when RunBench returns
        std::string adapterName;
        std::string driverVersion;
        uint32_t width = 0;
        uint32_t height = 0;
        bool tearingSupported = false;
        size_t objectCount = 0;
        size_t partCount = 0;       // of one object
        uint64_t triangleCount = 0; // of all objects: what one frame draws
        std::string lights;         // how many lights of each kind shine
    };

    // For run.txt: the lights the shaders get, e.g. "1 directional, 2 point, 2 spot"
    std::string DescribeLights(const engine::render::LightSet &lights) {
        const engine::render::PackedLightCounts counts = engine::render::CountPackedLights(lights);
        return std::format("{} directional, {} point, {} spot", counts.directional, counts.points, counts.spots);
    }

    // The model the grid draws: the --model file, or a cube. An error if there is nothing to draw.
    std::expected<const engine::render::Model *, std::string> LoadGridModel(const BenchOptions &options,
                                                                            engine::assets::AssetCache &assets) {
        if (!options.modelPath) {
            return &assets.CubeModel(assets.CheckerboardTexture());
        }
        const auto model = assets.LoadModel(*options.modelPath);
        if (!model) {
            return std::unexpected(model.error().message);
        }
        if ((*model)->parts.empty()) {
            return std::unexpected(
                std::format("'{}' has no triangles to draw", engine::core::ToUtf8(options.modelPath->native())));
        }
        return *model;
    }

    // Renders the warm-up frames, then the measured ones, in borderless fullscreen with vsync off and the camera on
    // its path. Nothing is logged or written while measuring. Esc stops the run. An error if the model cannot be used.
    std::expected<BenchRun, std::string> RunBench(const BenchOptions &options,
                                                  engine::rhi::ValidationStats &validation) {
        engine::core::Input input;
        engine::core::Window window({.title = L"D3D12 Engine - bench", .width = 1280, .height = 720});
        window.SetCallbacks({
            .onMessage =
                [&input](const engine::core::WindowMessage &message) {
                    input.HandleMessage(message);
                    return false; // let the window handle it as well
                },
        });
        // Before the renderer, so the swap chain starts at the monitor's size
        window.SetFullscreen(true);

        engine::render::Renderer renderer(window.Handle(), window.Width(), window.Height(), validation);
        if (!renderer.TearingSupported()) {
            LogWarning("Tearing not supported: Present without vsync may still wait for the display");
        }

        // Destroyed before the renderer, which owns what the cache created
        engine::assets::AssetCache assets(renderer.Resources());
        const std::expected<const engine::render::Model *, std::string> model = LoadGridModel(options, assets);
        if (!model) {
            return std::unexpected(model.error());
        }

        // Only the grid, no spinning cube
        engine::render::RenderList renderList; // empty for a static grid
        const engine::bench::StressGrid grid(renderer.Resources(), options.gridMode, options.count, **model);
        const std::array path = grid.CameraPath();
        engine::scene::Camera camera;
        camera.fovDegrees = 60.0f; // as in the sandbox
        // The default far plane (1000) would cut off the far side of a big grid
        camera.farZ = std::max(camera.farZ, 2.0f * grid.Extent() + 100.0f);

        const engine::render::RendererSettings settings{.drawPath = options.drawPath, .vsync = false};
        const engine::render::LightSet lights = grid.Lights();

        BenchRun run{.adapterName = renderer.AdapterName(),
                     .driverVersion = renderer.DriverVersion(),
                     .width = window.Width(),
                     .height = window.Height(),
                     .tearingSupported = renderer.TearingSupported(),
                     .objectCount = grid.ObjectCount(),
                     .partCount = (*model)->parts.size(),
                     .triangleCount = grid.ObjectCount() * (*model)->triangleCount,
                     .lights = DescribeLights(lights)};
        LogInfo("Bench: {}x{}, {} warm-up + {} measured frames", run.width, run.height, options.warmupFrames,
                options.frames);

        // Allocated up front, so measuring allocates nothing
        run.timings.resize(options.frames);
        const uint64_t totalFrames = uint64_t{options.warmupFrames} + options.frames;
        uint64_t frame = 0; // frames rendered so far
        Clock::time_point frameStart{};
        while (true) {
            // A frame's start is also the end of the previous frame
            const Clock::time_point now = Clock::now();
            if (frame > options.warmupFrames) {
                run.timings[frame - 1 - options.warmupFrames].cpuFrameMs = ToMs(now - frameStart);
            }
            frameStart = now;
            if (frame == totalFrames || !window.PumpMessages()) {
                break;
            }
            if (input.WasKeyPressed(VK_ESCAPE)) {
                window.Close(); // PumpMessages returns false next time
            }
            input.EndFrame();

            // The warm-up stays at the path's first pose
            const uint32_t pathFrame =
                frame < options.warmupFrames ? 0 : static_cast<uint32_t>(frame - options.warmupFrames);
            const engine::scene::CameraPose pose = engine::scene::EvaluateCameraPath(path, pathFrame);
            camera.position = pose.position;
            engine::scene::LookAt(camera, pose.target);

            renderer.BeginFrame(window.Width(), window.Height(), settings);
            const Clock::time_point recordStart = Clock::now();
            renderList.Clear();
            grid.Submit(renderList); // dynamic mode only
            renderer.Render(engine::scene::MakeFrameView(camera, renderer.AspectRatio()), lights, renderList,
                            grid.StaticLists());
            const Clock::time_point recordEnd = Clock::now();
            if (frame >= options.warmupFrames) {
                run.timings[frame - options.warmupFrames].cpuRecordMs = ToMs(recordEnd - recordStart);
            }
            renderer.EndFrame(); // vsync off (settings)
            ++frame;
        }

        run.result = {.exitCode = window.ExitCode(), .framesRendered = frame};
        return run;
    }

    // The lines of run.txt: settings, machine and the two medians
    std::vector<RunField> RunInfo(const BenchOptions &options, const BenchRun &run, std::string_view started,
                                  double medianFrameMs, double medianRecordMs) {
        const bool instanced = options.drawPath == engine::render::DrawPath::Instanced;
        return {
            {"engine", std::string(engine::bench::kEngineName)},
            {"scene", engine::core::ToUtf8(BenchSceneName(options).native())},
            {"grid", std::string(engine::apps::ToString(options.gridMode))},
            {"objects", std::format("{} (--count {})", run.objectCount, options.count)},
            {"model", options.modelPath ? engine::core::ToUtf8(options.modelPath->native()) : "cube"},
            {"parts", std::format("{}", run.partCount)},
            {"instances", std::format("{}", run.objectCount * run.partCount)},
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
        };
    }
} // namespace

// The benchmark: flies a scripted camera over a scene and writes the CPU time of every frame to
// results/dx12/<scene>/<timestamp>/. Runs unattended, so it never shows a dialog.
int WINAPI wWinMain(HINSTANCE /*hInst*/, HINSTANCE /*prevInst*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/) {
    // Always unattended: a failed D3D12MA assert must end the run, not wait at a dialog
    engine::apps::RouteCrtAssertsToStderr();
    try {
        const std::expected<BenchOptions, std::string> options = ParseBenchOptions(engine::core::CommandLineArgs());
        if (!options) {
            LogError("{}", options.error());
            LogInfo("{}", kBenchUsage);
            return engine::apps::kExitBadCommandLine;
        }
        LogInfo("Bench: {}, grid {}, --count {}, {}, results in '{}'",
                engine::core::ToUtf8(BenchSceneName(*options).native()), engine::apps::ToString(options->gridMode),
                options->count,
                options->drawPath == engine::render::DrawPath::Instanced ? "instanced" : "non-instanced",
                engine::core::ToUtf8(options->outDir.native()));
        const std::chrono::system_clock::time_point started = std::chrono::system_clock::now();

        // Image files are decoded through WIC, which needs COM
        const engine::core::ComScope com;

        // Outlives the renderer: its device writes the live-object count when it is destroyed
        engine::rhi::ValidationStats validation;
        const std::expected<BenchRun, std::string> outcome = RunBench(*options, validation);
        engine::apps::LogValidationSummary(validation);
        if (!outcome) {
            LogError("{}; nothing written", outcome.error());
            return kExitBenchFailed;
        }
        const BenchRun &run = *outcome;

        const uint64_t expectedFrames = uint64_t{options->warmupFrames} + options->frames;
        if (run.result.framesRendered != expectedFrames) {
            LogError("Bench stopped early: {} of {} frames rendered; nothing written", run.result.framesRendered,
                     expectedFrames);
            return kExitBenchFailed;
        }

        std::vector<double> frameMs;
        std::vector<double> recordMs;
        frameMs.reserve(run.timings.size());
        recordMs.reserve(run.timings.size());
        for (const FrameTiming &timing : run.timings) {
            frameMs.push_back(timing.cpuFrameMs);
            recordMs.push_back(timing.cpuRecordMs);
        }
        const double medianFrameMs = Median(std::move(frameMs));
        const double medianRecordMs = Median(std::move(recordMs));
        LogInfo("Measured {} frames: cpu_frame_ms median {:.3f}, cpu_record_ms median {:.3f}", run.timings.size(),
                medianFrameMs, medianRecordMs);

        const std::string timestamp = engine::bench::FolderTimestamp(started);
        const std::vector<RunField> fields = RunInfo(*options, run, timestamp, medianFrameMs, medianRecordMs);
        const std::expected<std::filesystem::path, std::string> folder =
            engine::bench::WriteResults(options->outDir, BenchSceneName(*options), timestamp, run.timings, fields);
        if (!folder) {
            LogError("Results not written: {}", folder.error());
            return kExitBenchFailed;
        }
        LogInfo("Results: {}", engine::core::ToUtf8(folder->native()));

#if defined(_DEBUG)
        // With the debug layer, a run with errors or leaks fails like the sandbox's smoke test
        return engine::apps::SmokeTestExitCode(expectedFrames, run.result, validation);
#else
        return run.result.exitCode;
#endif
    } catch (const std::exception &e) {
        LogError("{}", e.what());
        return -1;
    }
}
