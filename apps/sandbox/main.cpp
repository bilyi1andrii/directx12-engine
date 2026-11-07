#include <format>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "SandboxOptions.h"
#include "assets/TextureLoader.h"
#include "core/CommandLine.h"
#include "core/ComScope.h"
#include "core/Log.h"
#include "core/ThrowIfFailed.h"
#include "core/Timer.h"
#include "core/Window.h"
#include "core/Input.h"
#include "core/Assert.h"
#include "core/Error.h"
#include "rhi/AgilitySdk.h"
#include "rhi/ValidationStats.h"
#include "render/Renderer.h"
#include "scene/Camera.h"
#include "scene/CameraController.h"

using engine::core::LogError;
using engine::core::LogInfo;
using engine::core::LogWarning;

ENGINE_EXPORT_AGILITY_SDK()

namespace {
    // Exit codes besides the window's own (0). An assert exits with 3, an exception with -1.
    constexpr int kExitSmokeTestFailed = 1;
    constexpr int kExitBadCommandLine = 2;

    // The debug layer exists only in Debug builds (rhi::Device checks the same macro)
#if defined(_DEBUG)
    constexpr bool kValidationExpected = true;
#else
    constexpr bool kValidationExpected = false;
#endif

    // The sandbox scene: a square grid of cubes below the origin (--count, default 20 x 20) and one cube
    // spinning at the origin
    constexpr float kGridSpacing = 3.0f; // the cube is 2 units wide, so 1 unit of gap
    constexpr float kGridHeight = -4.0f;

    // Side of the grid for --count: the smallest n with n * n >= count, so the count is rounded up to a
    // square (64-bit product: the square of a large uint32_t does not fit in one)
    uint32_t GridSide(uint32_t count) {
        uint32_t side = 0;
        while (uint64_t{side} * side < count) {
            ++side;
        }
        return side;
    }

    // The renderer's input for one frame, from the camera (scene and render do not know each other)
    engine::render::FrameView MakeFrameView(const engine::scene::Camera &camera, float aspectRatio) {
        engine::render::FrameView view{};
        DirectX::XMStoreFloat4x4(&view.view, engine::scene::ViewMatrix(camera));
        DirectX::XMStoreFloat4x4(&view.projection, engine::scene::ProjectionMatrix(camera, aspectRatio));
        view.cameraPosition = camera.position;
        return view;
    }

    // A 256x256 light and dark grey checkerboard. Without mips, distant cubes would shimmer.
    std::expected<engine::render::TextureData, engine::core::Error> MakeCheckerboard() {
        constexpr uint32_t kSize = 256;
        constexpr uint32_t kSquare = 32;
        std::vector<std::byte> rgba8(size_t{kSize} * kSize * 4);
        for (uint32_t y = 0; y < kSize; ++y) {
            for (uint32_t x = 0; x < kSize; ++x) {
                const bool light = (x / kSquare + y / kSquare) % 2 == 0;
                const std::byte value{light ? uint8_t{220} : uint8_t{60}};
                std::byte *pixel = &rgba8[(size_t{y} * kSize + x) * 4];
                pixel[0] = value;
                pixel[1] = value;
                pixel[2] = value;
                pixel[3] = std::byte{255};
            }
        }
        return engine::assets::CreateTextureFromPixels(rgba8, kSize, kSize, engine::assets::TextureUsage::Diffuse);
    }

    // The grid's texture: the --texture file or the checkerboard. If loading fails, a warning and grey.
    engine::render::TextureHandle CreateGridTexture(engine::render::Renderer &renderer, const SandboxOptions &options) {
        const std::expected<engine::render::TextureData, engine::core::Error> texture =
            options.texturePath
                ? engine::assets::LoadTexture(*options.texturePath, engine::assets::TextureUsage::Diffuse)
                : MakeCheckerboard();
        if (!texture) {
            LogWarning("Grid texture: {}. Using the grey fallback.", texture.error().message);
            return renderer.UnloadedTexture();
        }
        const std::wstring name =
            options.texturePath ? L"Grid/" + options.texturePath->filename().wstring() : L"Grid/Checkerboard";
        return renderer.CreateTexture(*texture, name);
    }

    struct RunResult {
        int exitCode = 0; // the window's
        uint64_t framesRendered = 0;
    };

    // Creates the window and the renderer and runs the game loop until the window closes
    // (in a smoke test: after the requested number of frames). Everything it creates is
    // destroyed before it returns, so `validation` is complete afterwards.
    RunResult RunSandbox(const SandboxOptions &options, engine::rhi::ValidationStats &validation) {
        engine::core::Timer initTimer;
        initTimer.Start();

        engine::core::Input input;

        engine::core::Window window({.title = L"D3D12 Engine", .width = 1280, .height = 720});

        // No onResize: one fullscreen toggle sends several WM_SIZE messages, so the swap chain
        // follows Window::Width()/Height() once per frame instead
        window.SetCallbacks({
            .onMessage =
                [&input](const engine::core::WindowMessage &message) {
                    input.HandleMessage(message);
                    return false; // let the window handle it as well
                },
        });

        engine::render::Renderer renderer(window.Handle(), window.Width(), window.Height(), validation);

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

        // What to draw, rebuilt every frame like the DX11 dynamic buckets: the textured grid and the
        // vertex-colour spinning cube, each in its own bucket. A material is its texture for now.
        engine::render::RenderList renderList;
        const engine::render::TextureHandle gridTexture = CreateGridTexture(renderer, options);
        const engine::render::BatchKey gridKey{.mesh = renderer.TexturedCubeMesh(),
                                               .material = {.index = gridTexture.index}};
        const engine::render::BatchKey spinningCubeKey{.mesh = renderer.CubeMesh(), .material = {}};
        DirectX::XMFLOAT4X4 gridBase{};
        DirectX::XMStoreFloat4x4(&gridBase, DirectX::XMMatrixTranslation(0.0f, kGridHeight, 0.0f));
        // e.g. --count 50000 gives 224 x 224 = 50,176 cubes; the title shows the real object count
        const uint32_t gridSide = GridSide(options.gridCount.value_or(kDefaultGridCount));
        LogInfo("Grid: {} x {} = {} cubes", gridSide, gridSide, uint64_t{gridSide} * gridSide);
        DirectX::XMFLOAT4X4 spinningCubeWorld{};

        engine::render::RendererSettings settings; // draw path toggled with I

        // Game loop: handle all pending window messages, update, render
        engine::core::FrameTimer frameTimer;
        bool vsync = true; // toggled with V
        uint64_t framesRendered = 0;
        while (window.PumpMessages()) {
            frameTimer.Tick();
            if (input.WasKeyPressed(VK_ESCAPE)) {
                window.Close();
            }
            if (input.WasKeyPressed('V')) {
                vsync = !vsync;
                LogInfo("VSync {}", vsync ? "on" : "off");
            }
            if (input.WasKeyPressed('I')) {
                settings.drawPath = (settings.drawPath == engine::render::DrawPath::Instanced)
                                        ? engine::render::DrawPath::NonInstanced
                                        : engine::render::DrawPath::Instanced;
                LogInfo("Draw path: {}",
                        settings.drawPath == engine::render::DrawPath::Instanced ? "instanced" : "non-instanced");
            }

            if (input.WasKeyPressed(VK_F11) || (input.WasKeyPressed(VK_RETURN) && input.IsKeyDown(VK_MENU))) {
                window.ToggleFullscreen();
            }

            cameraController.Update(camera, input, frameTimer.DeltaSeconds());

            if (frameTimer.FpsUpdated()) {
                const double fps = frameTimer.Fps();
                // The object count is the previous frame's list; the scene does not change size
                window.SetTitle(std::format(
                    L"D3D12 Engine - {:.1f} FPS ({:.2f} ms) - {}, {} objects", fps, 1000.0 / fps,
                    settings.drawPath == engine::render::DrawPath::Instanced ? L"instanced" : L"non-instanced",
                    renderList.InstanceCount()));
            }

            // Everything above has read this frame's input; clear the
            // per-frame values (pressed, deltas).
            input.EndFrame();

            // A minimized window has nothing to show.
            // WaitMessage() sleeps until the next message.
            // A smoke test keeps rendering, or it would never reach its frame count: the swap chain
            // ignores the 0x0 size, and Present returns DXGI_STATUS_OCCLUDED, which is a success code.
            if (window.IsMinimized() && !smokeTestFrames) {
                ::WaitMessage();
                continue;
            }
            // 90 degrees per second around the (0, 1, 1) axis, like the step-1 cube. fmod in double keeps
            // the angle precise however long the program runs.
            const float angle =
                DirectX::XMConvertToRadians(static_cast<float>(std::fmod(frameTimer.TotalSeconds() * 90.0, 360.0)));
            DirectX::XMStoreFloat4x4(
                &spinningCubeWorld, DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 1.0f, 0.0f), angle));

            renderList.Clear();
            engine::render::SubmitGrid(renderList, gridKey, gridBase, gridSide, gridSide, kGridSpacing, kGridSpacing);
            renderList.Add(spinningCubeKey, spinningCubeWorld);

            try {
                renderer.BeginFrame(window.Width(), window.Height(), settings);
                // After BeginFrame: the aspect ratio follows this frame's swap chain size
                renderer.Render(MakeFrameView(camera, renderer.AspectRatio()), renderList);
                renderer.EndFrame(vsync);
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

    void LogValidationSummary(const engine::rhi::ValidationStats &validation) {
        if (!validation.active) {
            if (kValidationExpected) {
                LogWarning("Validation: not active (D3D12 or DXGI debug interface unavailable)");
            } else {
                LogInfo("Validation: not active (no debug layer in this build)");
            }
            return;
        }
        const uint32_t errors = validation.errorCount.load();
        if (errors == 0 && validation.liveObjectCount == 0) {
            LogInfo("Validation: 0 debug-layer errors, 0 live objects at shutdown");
        } else {
            LogWarning("Validation: {} debug-layer errors, {} live objects at shutdown", errors,
                       validation.liveObjectCount);
        }
    }

    // Checks a finished smoke test and logs the verdict. Returns the process exit code.
    int SmokeTestExitCode(uint32_t expectedFrames, const RunResult &run,
                          const engine::rhi::ValidationStats &validation) {
        std::vector<std::string> failures;
        if (run.framesRendered != expectedFrames) {
            // e.g. the window was closed early (Alt+F4, WM_CLOSE from outside)
            failures.push_back(std::format("rendered {} of {} frames", run.framesRendered, expectedFrames));
        }
        if (run.exitCode != 0) {
            failures.push_back(std::format("window exit code {}", run.exitCode));
        }
        if (validation.active) {
            const uint32_t errors = validation.errorCount.load();
            if (errors != 0) {
                failures.push_back(std::format("{} debug-layer errors", errors));
            }
            if (validation.liveObjectCount != 0) {
                failures.push_back(std::format("{} live objects at shutdown", validation.liveObjectCount));
            }
        } else if (kValidationExpected) {
            // Fail closed: a Debug run that could not check anything must not pass
            failures.emplace_back("validation was not active");
        } else {
            LogWarning("Smoke test: no debug layer in this build; only checks that {} frames render and the app "
                       "shuts down",
                       expectedFrames);
        }

        if (failures.empty()) {
            LogInfo("Smoke test PASSED: {} frames", run.framesRendered);
            return 0;
        }
        std::string reasons;
        for (const std::string &failure : failures) {
            reasons += reasons.empty() ? failure : "; " + failure;
        }
        LogError("Smoke test FAILED: {}", reasons);
        return kExitSmokeTestFailed;
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
            return kExitBadCommandLine;
        }
        smokeTest = options->smokeTestFrames.has_value();

        // For the whole run: image files are decoded through WIC, which needs COM
        const engine::core::ComScope com;

        // Outlives the renderer: its device writes the live-object count when it is destroyed
        engine::rhi::ValidationStats validation;
        const RunResult run = RunSandbox(*options, validation);
        LogValidationSummary(validation);

        const int exitCode = smokeTest ? SmokeTestExitCode(*options->smokeTestFrames, run, validation) : run.exitCode;
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
