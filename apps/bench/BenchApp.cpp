#include "BenchApp.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <optional>
#include <utility>

#include <Windows.h>

#include "core/Input.h"
#include "core/Log.h"
#include "core/Window.h"
#include "render/GpuResources.h"
#include "render/Renderer.h"
#include "renderdata/LightPacking.h"
#include "renderdata/Material.h"
#include "rhi/GpuTimes.h"

namespace chrono = std::chrono;

using core::LogInfo;

namespace bench {
    namespace {
        using Clock = chrono::steady_clock;

        double ToMs(Clock::duration duration) {
            return chrono::duration<double, std::milli>(duration).count();
        }

        std::string DescribeLights(const render::LightSet &lights) {
            const render::PackedLightCounts counts = render::CountPackedLights(lights);
            return std::format("{} directional, {} point, {} spot", counts.directional, counts.points, counts.spots);
        }

        scene::Camera MakeCamera(const StressGrid &grid) {
            scene::Camera camera;
            camera.fovDegrees = 60.0f;
            camera.farZ = std::max(camera.farZ, 2.0f * grid.Extent() + 100.0f);
            return camera;
        }

        BenchRun DescribeRun(apps::AppShell &shell, const StressGrid &grid, const render::Model &model,
                             const render::LightSet &lights, uint32_t measuredFrames) {
            render::Renderer &renderer = shell.Renderer();
            BenchRun run{.adapterName = renderer.AdapterName(),
                         .driverVersion = renderer.DriverVersion(),
                         .width = shell.Window().Width(),
                         .height = shell.Window().Height(),
                         .tearingSupported = renderer.TearingSupported(),
                         .objectCount = grid.ObjectCount(),
                         .partCount = model.parts.size(),
                         .triangleCount = grid.ObjectCount() * model.triangleCount,
                         .lights = DescribeLights(lights)};
            for (const render::ModelPart &part : model.parts) {
                const render::MaterialInfo &material = renderer.Resources().Material(part.material);
                if (material.alphaMode == render::AlphaMode::Blend) {
                    ++run.transparentPartCount;
                    run.transparentDrawsPerObject += material.doubleSided ? 2 : 1;
                }
            }
            run.timings.resize(measuredFrames);
            return run;
        }
    } // namespace

    BenchApp::BenchApp(apps::AppShell &shell, const BenchOptions &options, const render::Model &model)
        : m_shell(shell), m_options(options),
          m_grid(shell.Renderer().Resources(), options.gridMode, options.count, model), m_path(m_grid.CameraPath()),
          m_camera(MakeCamera(m_grid)), m_settings{.drawPath = options.drawPath, .vsync = false},
          m_lights(m_grid.Lights()), m_run(DescribeRun(shell, m_grid, model, m_lights, options.frames)),
          m_gpuTimes(m_run.timings, options.warmupFrames) {
        LogInfo("Bench: {}x{}, {} warm-up + {} measured frames", m_run.width, m_run.height, m_options.warmupFrames,
                m_options.frames);
    }

    std::expected<BenchRun, std::string> BenchApp::Run() {
        Measure();

        for (const rhi::GpuFrameTimes &times : m_shell.Renderer().FinishGpuTimes()) {
            m_gpuTimes.Add(times);
        }
        m_run.result = {.exitCode = m_shell.Window().ExitCode(), .framesRendered = m_frame};
        if (m_frame == TotalFrames()) {
            if (const std::expected<void, std::string> checked = m_gpuTimes.Check(); !checked) {
                return std::unexpected("GPU times: " + checked.error());
            }
        }
        m_run.gpuPasses.assign(m_gpuTimes.Passes().begin(), m_gpuTimes.Passes().end());
        return std::move(m_run);
    }

    void BenchApp::Measure() {
        core::Window &window = m_shell.Window();
        Clock::time_point frameStart{};
        while (true) {
            const Clock::time_point now = Clock::now();
            if (m_frame > m_options.warmupFrames) {
                m_run.timings[m_frame - 1 - m_options.warmupFrames].cpuFrameMs = ToMs(now - frameStart);
            }
            frameStart = now;
            if (m_frame == TotalFrames() || !window.PumpMessages()) {
                break;
            }
            HandleKeys();
            PlaceCamera(m_frame);
            RenderFrame(m_frame);
            ++m_frame;
        }
    }

    void BenchApp::HandleKeys() {
        core::Input &input = m_shell.Input();
        if (input.WasKeyPressed(VK_ESCAPE)) {
            m_shell.Window().Close();
        }
        input.EndFrame();
    }

    void BenchApp::PlaceCamera(uint64_t frame) {
        const uint32_t pathFrame =
            frame < m_options.warmupFrames ? 0 : static_cast<uint32_t>(frame - m_options.warmupFrames);
        const scene::CameraPose pose = scene::EvaluateCameraPath(m_path, pathFrame);
        m_camera.position = pose.position;
        scene::LookAt(m_camera, pose.target);
    }

    void BenchApp::RenderFrame(uint64_t frame) {
        render::Renderer &renderer = m_shell.Renderer();
        const core::Window &window = m_shell.Window();
        renderer.BeginFrame(window.Width(), window.Height(), m_settings);
        if (const std::optional<rhi::GpuFrameTimes> &gpu = renderer.GpuTimes()) {
            m_gpuTimes.Add(*gpu);
        }
        const Clock::time_point recordStart = Clock::now();
        m_renderList.Clear();
        m_grid.Submit(m_renderList);
        renderer.Render(scene::MakeFrameView(m_camera, renderer.AspectRatio()), m_lights, m_renderList,
                        m_grid.StaticLists());
        const Clock::time_point recordEnd = Clock::now();
        if (frame >= m_options.warmupFrames) {
            m_run.timings[frame - m_options.warmupFrames].cpuRecordMs = ToMs(recordEnd - recordStart);
        }
        renderer.EndFrame();
    }
} // namespace bench
