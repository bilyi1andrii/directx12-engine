#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "BenchOptions.h"
#include "common/AppChecks.h"
#include "common/StressScene.h"
#include "core/CommandLine.h"
#include "core/ComScope.h"
#include "core/Input.h"
#include "core/Log.h"
#include "core/ThrowIfFailed.h"
#include "core/Utf8.h"
#include "core/Window.h"
#include "rhi/AgilitySdk.h"
#include "rhi/ValidationStats.h"
#include "render/Renderer.h"
#include "scene/Camera.h"
#include "scene/CameraPath.h"

using engine::core::LogError;
using engine::core::LogInfo;
using engine::core::LogWarning;

ENGINE_EXPORT_AGILITY_SDK()

namespace {
    using Clock = std::chrono::steady_clock;

    // The run stopped before all frames were rendered (Esc, window closed)
    constexpr int kExitBenchStoppedEarly = 1;

    // One measured frame (milliseconds)
    struct FrameTiming {
        // From the start of this frame to the start of the next: everything, including the fence wait in
        // BeginFrame, Present and the message pump
        double cpuFrameMs = 0.0;
        // Building this frame's dynamic list + Renderer::Render: extraction, instance upload, command recording
        double cpuRecordMs = 0.0;
    };

    double ToMs(Clock::duration duration) {
        return std::chrono::duration<double, std::milli>(duration).count();
    }

    // Median of `values` (a copy: nth_element reorders it). The rules ask for medians, not means.
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
        std::vector<FrameTiming> timings; // options.frames entries, all filled if the run completed
    };

    // Renders options.warmupFrames + options.frames frames of the stress scene in borderless fullscreen with
    // vsync off, the camera on the scripted path, and measures the last options.frames of them. Nothing is
    // logged or written while measuring. Esc or closing the window stops the run early.
    BenchRun RunBench(const BenchOptions &options, engine::rhi::ValidationStats &validation) {
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

        // The scene: the stress grid only (no spinning cube), and the camera on its path
        engine::render::RenderList renderList; // this frame's dynamic objects (empty for a static grid)
        const engine::apps::StressGrid grid(renderer, options.gridMode, options.count,
                                            engine::apps::CreateGridTexture(renderer, std::nullopt));
        const std::array path = grid.CameraPath();
        engine::scene::Camera camera;
        camera.fovDegrees = 60.0f; // as in the sandbox
        // Far enough for the whole grid from anywhere on the path (the default 1000 cuts off big grids)
        camera.farZ = std::max(camera.farZ, 2.0f * grid.Extent() + 100.0f);

        const engine::render::RendererSettings settings{.drawPath = options.drawPath};

        LogInfo("Bench: {}x{}, {} warm-up + {} measured frames", window.Width(), window.Height(), options.warmupFrames,
                options.frames);

        // Preallocated, so measuring allocates nothing
        std::vector<FrameTiming> timings(options.frames);
        const uint64_t totalFrames = uint64_t{options.warmupFrames} + options.frames;
        uint64_t frame = 0; // frames rendered so far
        Clock::time_point frameStart{};
        while (true) {
            // The start of frame `frame` is also the end of the one before it
            const Clock::time_point now = Clock::now();
            if (frame > options.warmupFrames) {
                timings[frame - 1 - options.warmupFrames].cpuFrameMs = ToMs(now - frameStart);
            }
            frameStart = now;
            if (frame == totalFrames || !window.PumpMessages()) {
                break;
            }
            if (input.WasKeyPressed(VK_ESCAPE)) {
                window.Close(); // PumpMessages returns false next time
            }
            input.EndFrame();

            // The path is indexed by measured frame; the warm-up stays at its first pose
            const uint32_t pathFrame =
                frame < options.warmupFrames ? 0 : static_cast<uint32_t>(frame - options.warmupFrames);
            const engine::scene::CameraPose pose = engine::scene::EvaluateCameraPath(path, pathFrame);
            camera.position = pose.position;
            engine::scene::LookAt(camera, pose.target);

            try {
                renderer.BeginFrame(window.Width(), window.Height(), settings);
                const Clock::time_point recordStart = Clock::now();
                renderList.Clear();
                grid.Submit(renderList); // dynamic mode only
                renderer.Render(engine::apps::MakeFrameView(camera, renderer.AspectRatio()), renderList,
                                grid.StaticLists());
                const Clock::time_point recordEnd = Clock::now();
                if (frame >= options.warmupFrames) {
                    timings[frame - options.warmupFrames].cpuRecordMs = ToMs(recordEnd - recordStart);
                }
                renderer.EndFrame(false); // vsync off, with tearing when supported
            } catch (const engine::core::HResultError &) {
                renderer.LogDeviceRemovedInfo();
                throw;
            }
            ++frame;
        }

        return {.result = {.exitCode = window.ExitCode(), .framesRendered = frame}, .timings = std::move(timings)};
    }
} // namespace

// The benchmark: runs a scene preset along a scripted camera path with fixed settings and measures per-frame
// CPU times (plan section 3.7, rules in .claude/rules/bench-and-tools.md). Unattended: no dialogs.
int WINAPI wWinMain(HINSTANCE /*hInst*/, HINSTANCE /*prevInst*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/) {
    try {
        const std::expected<BenchOptions, std::string> options = ParseBenchOptions(engine::core::CommandLineArgs());
        if (!options) {
            LogError("{}", options.error());
            LogInfo("{}", kBenchUsage);
            return engine::apps::kExitBadCommandLine;
        }
        LogInfo("Bench: scene {}, grid {}, {} cubes, {}, results in '{}'", ToString(options->scene),
                engine::apps::ToString(options->gridMode), options->count,
                options->drawPath == engine::render::DrawPath::Instanced ? "instanced" : "non-instanced",
                engine::core::ToUtf8(options->outDir.native()));

        // For the whole run: image files are decoded through WIC, which needs COM (Sponza later)
        const engine::core::ComScope com;

        // Outlives the renderer: its device writes the live-object count when it is destroyed
        engine::rhi::ValidationStats validation;
        const BenchRun run = RunBench(*options, validation);
        engine::apps::LogValidationSummary(validation);

        const uint64_t expectedFrames = uint64_t{options->warmupFrames} + options->frames;
        if (run.result.framesRendered != expectedFrames) {
            LogError("Bench stopped early: {} of {} frames rendered; nothing measured", run.result.framesRendered,
                     expectedFrames);
            return kExitBenchStoppedEarly;
        }

        std::vector<double> frameMs;
        std::vector<double> recordMs;
        frameMs.reserve(run.timings.size());
        recordMs.reserve(run.timings.size());
        for (const FrameTiming &timing : run.timings) {
            frameMs.push_back(timing.cpuFrameMs);
            recordMs.push_back(timing.cpuRecordMs);
        }
        LogInfo("Measured {} frames: cpu_frame_ms median {:.3f}, cpu_record_ms median {:.3f}", run.timings.size(),
                Median(std::move(frameMs)), Median(std::move(recordMs)));
        return run.result.exitCode;
    } catch (const std::exception &e) {
        LogError("{}", e.what());
        return -1;
    }
}
