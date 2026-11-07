#include <format>

#include <cmath>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

#include "SandboxOptions.h"
#include "common/AppChecks.h"
#include "common/StressScene.h"
#include "core/CommandLine.h"
#include "core/ComScope.h"
#include "core/Log.h"
#include "core/ThrowIfFailed.h"
#include "core/Timer.h"
#include "core/Window.h"
#include "core/Input.h"
#include "rhi/AgilitySdk.h"
#include "rhi/ValidationStats.h"
#include "render/Renderer.h"
#include "scene/Camera.h"
#include "scene/CameraController.h"
#include "ui/DebugUI.h"

using engine::core::LogError;
using engine::core::LogInfo;

ENGINE_EXPORT_AGILITY_SDK()

namespace {
    // Logs what the renderer window or a shortcut changed this frame
    void LogSettingsChanges(const engine::render::RendererSettings &before,
                            const engine::render::RendererSettings &after) {
        if (after.drawPath != before.drawPath) {
            LogInfo("Draw path: {}",
                    after.drawPath == engine::render::DrawPath::Instanced ? "instanced" : "non-instanced");
        }
        if (after.vsync != before.vsync) {
            LogInfo("VSync {}", after.vsync ? "on" : "off");
        }
    }

    // Creates the window and the renderer and runs the game loop until the window closes
    // (in a smoke test: after the requested number of frames). Everything it creates is
    // destroyed before it returns, so `validation` is complete afterwards.
    engine::apps::RunResult RunSandbox(const SandboxOptions &options, engine::rhi::ValidationStats &validation) {
        engine::core::Timer initTimer;
        initTimer.Start();

        engine::core::Input input;

        engine::core::Window window({.title = L"D3D12 Engine", .width = 1280, .height = 720});

        // After the window (its backend needs the HWND), before the renderer (the renderer's ImGui backend needs
        // the context, also while the renderer is destroyed)
        engine::ui::DebugUI ui(window);

        // No onResize: one fullscreen toggle sends several WM_SIZE messages, so the swap chain
        // follows Window::Width()/Height() once per frame instead
        window.SetCallbacks({
            .onMessage =
                [&input](const engine::core::WindowMessage &message) {
                    // ImGui first. DebugUI::HandleMessage is static and does nothing once `ui` is gone, so this
                    // stays safe while the window, which outlives `ui`, is destroyed.
                    const bool handledByUi = engine::ui::DebugUI::HandleMessage(message);
                    input.HandleMessage(message);
                    // Only true for WM_SETCURSOR over ImGui (and IME text): the window must not reset the cursor
                    return handledByUi;
                },
        });

        engine::render::Renderer renderer(window.Handle(), window.Width(), window.Height(), validation);
        renderer.EnableUi();

        LogInfo("Initialized in {:.0f} ms: {}x{}, tearing {}", initTimer.ElapsedMs(), window.Width(), window.Height(),
                renderer.TearingSupported() ? "supported" : "not supported");

        const std::optional<uint32_t> smokeTestFrames = options.smokeTestFrames;
        if (smokeTestFrames) {
            LogInfo("Smoke test: rendering {} frames", *smokeTestFrames);
        }

        // Looks at the cube from above and to the side, so three faces are visible
        engine::scene::Camera camera;
        camera.position = {3.0f, 4.0f, -8.0f};
        engine::scene::LookAt(camera, {0.0f, 0.0f, 0.0f});
        camera.fovDegrees = 60.0f;
        const engine::scene::CameraController cameraController;

        // The textured grid (--count, --grid) and a vertex-colour cube spinning at the origin. The spinning cube is
        // uploaded every frame, the grid only in dynamic mode.
        engine::render::RenderList renderList; // this frame's dynamic objects
        const engine::apps::StressGrid grid(renderer, options.gridMode.value_or(engine::apps::GridMode::Dynamic),
                                            options.gridCount.value_or(engine::apps::kDefaultGridCount),
                                            engine::apps::CreateGridTexture(renderer, options.texturePath));
        const engine::render::BatchKey spinningCubeKey{.mesh = renderer.CubeMesh(), .material = {}};
        DirectX::XMFLOAT4X4 spinningCubeWorld{};

        // Edited by the renderer window and the shortcuts I and V; the renderer takes a copy in BeginFrame
        engine::render::RendererSettings settings;

        // For the renderer window; the grid does not change while the sandbox runs
        const std::string sceneInfo =
            std::format("grid {} x {}, {}", grid.Side(), grid.Side(), engine::apps::ToString(grid.Mode()));

        // Game loop: handle all pending window messages, update, render
        engine::core::FrameTimer frameTimer;
        uint64_t framesRendered = 0;
        while (window.PumpMessages()) {
            frameTimer.Tick();

            // A minimized window has nothing to show.
            // WaitMessage() sleeps until the next message.
            // A smoke test keeps rendering, or it would never reach its frame count: the swap chain
            // ignores the 0x0 size, and Present returns DXGI_STATUS_OCCLUDED, which is a success code.
            // Before the UI frame, so that every NewFrame gets its Render.
            if (window.IsMinimized() && !smokeTestFrames) {
                input.EndFrame();
                ::WaitMessage();
                continue;
            }

            // The UI frame starts first: NewFrame decides which input ImGui uses this frame, and the app ignores
            // that input (no camera look while dragging a window, no shortcuts while typing into a widget)
            ui.NewFrame();
            const bool appKeyboard = !ui.WantsKeyboard();
            const bool appMouse = !ui.WantsMouse();

            const engine::render::RendererSettings previousSettings = settings;
            if (appKeyboard) {
                if (input.WasKeyPressed(VK_ESCAPE)) {
                    window.Close();
                }
                if (input.WasKeyPressed('V')) {
                    settings.vsync = !settings.vsync;
                }
                if (input.WasKeyPressed('I')) {
                    settings.drawPath = (settings.drawPath == engine::render::DrawPath::Instanced)
                                            ? engine::render::DrawPath::NonInstanced
                                            : engine::render::DrawPath::Instanced;
                }
                if (input.WasKeyPressed(VK_F11) || (input.WasKeyPressed(VK_RETURN) && input.IsKeyDown(VK_MENU))) {
                    window.ToggleFullscreen();
                }
            }

            cameraController.Update(camera, input, frameTimer.DeltaSeconds(),
                                    {.mouse = appMouse, .keyboard = appKeyboard});

            ui.ShowRendererWindow({.fps = frameTimer.Fps(),
                                   .objectCount = grid.ObjectCount() + 1, // the grid plus the spinning cube
                                   .scene = sceneInfo,
                                   .cameraPosition = camera.position},
                                  settings);
            ImDrawData *const uiDrawData = ui.Render();
            LogSettingsChanges(previousSettings, settings);

            // Everything above has read this frame's input; clear the
            // per-frame values (pressed, deltas).
            input.EndFrame();

            // 90 degrees per second around the (0, 1, 1) axis, like the step-1 cube. fmod in double keeps
            // the angle precise however long the program runs.
            const float angle =
                DirectX::XMConvertToRadians(static_cast<float>(std::fmod(frameTimer.TotalSeconds() * 90.0, 360.0)));
            DirectX::XMStoreFloat4x4(
                &spinningCubeWorld, DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 1.0f, 0.0f), angle));

            renderList.Clear();
            grid.Submit(renderList); // dynamic mode only
            renderList.Add(spinningCubeKey, spinningCubeWorld);

            try {
                renderer.BeginFrame(window.Width(), window.Height(), settings);
                // After BeginFrame: the aspect ratio follows this frame's swap chain size
                renderer.Render(engine::apps::MakeFrameView(camera, renderer.AspectRatio()), renderList,
                                grid.StaticLists());
                renderer.RenderUi(uiDrawData); // on top of the scene
                renderer.EndFrame();
            } catch (const engine::core::HResultError &) {
                // Reset, ResizeBuffers, Present, Signal and others fail with
                // DXGI_ERROR_DEVICE_REMOVED once the GPU is lost
                renderer.LogDeviceRemovedInfo();
                throw;
            }
            ++framesRendered;

            if (smokeTestFrames) {
                const uint32_t frames = *smokeTestFrames;
                // Two fullscreen toggles exercise the resize path: flush, ResizeBuffers, new RTVs and
                // the deferred release of the old depth buffer
                if (frames >= 3 && (framesRendered == frames / 3 || framesRendered == 2 * frames / 3)) {
                    window.ToggleFullscreen();
                }
                // The second half runs on the other draw path, so both stay covered by the smoke test
                if (framesRendered == frames / 2) {
                    settings.drawPath = engine::render::DrawPath::NonInstanced;
                    LogInfo("Smoke test: draw path non-instanced from frame {}", framesRendered + 1);
                }
                if (framesRendered == frames) {
                    window.Close();
                }
            }
        }

        return {.exitCode = window.ExitCode(), .framesRendered = framesRendered};
    }
} // namespace

int WINAPI wWinMain(HINSTANCE /*hInst*/, HINSTANCE /*prevInst*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/) {
    // Known before anything that can fail later; the catch below needs it
    bool smokeTest = false;
    try {
        const std::expected<SandboxOptions, std::string> options = ParseSandboxOptions(engine::core::CommandLineArgs());
        if (!options) {
            // No dialog: a wrong command line usually comes from a script
            LogError("{}", options.error());
            LogInfo("{}", kSandboxUsage);
            return engine::apps::kExitBadCommandLine;
        }
        smokeTest = options->smokeTestFrames.has_value();
        if (smokeTest) {
            // A failed ImGui or D3D12MA assert must end the run, not wait at a dialog until ctest's timeout
            engine::apps::RouteCrtAssertsToStderr();
        }

        // For the whole run: image files are decoded through WIC, which needs COM
        const engine::core::ComScope com;

        // Outlives the renderer: its device writes the live-object count when it is destroyed
        engine::rhi::ValidationStats validation;
        const engine::apps::RunResult run = RunSandbox(*options, validation);
        engine::apps::LogValidationSummary(validation);

        const int exitCode =
            smokeTest ? engine::apps::SmokeTestExitCode(*options->smokeTestFrames, run, validation) : run.exitCode;
        LogInfo("Exiting with code {}", exitCode);
        return exitCode;
    } catch (std::exception &e) {
        LogError("{}", e.what());
        // A dialog would block an unattended run until someone closes it
        if (!smokeTest) {
            MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR);
        }
        return -1;
    }
}
