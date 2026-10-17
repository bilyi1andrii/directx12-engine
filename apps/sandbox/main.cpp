#include <format>

#include "core/Log.h"
#include "core/ThrowIfFailed.h"
#include "core/Timer.h"
#include "core/Window.h"
#include "core/Input.h"
#include "core/Assert.h"
#include "rhi/AgilitySdk.h"
#include "rhi/ValidationStats.h"
#include "render/Renderer.h"

using engine::core::LogError;
using engine::core::LogInfo;
using engine::core::LogWarning;

ENGINE_EXPORT_AGILITY_SDK()

namespace {
    // Creates the window and the renderer and runs the game loop until the window closes.
    // Returns the window's exit code. Everything it creates is destroyed before it returns,
    // so `validation` is complete afterwards.
    int RunSandbox(engine::rhi::ValidationStats &validation) {
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

        // Game loop: handle all pending window messages, update, render
        engine::core::FrameTimer frameTimer;
        bool vsync = true;            // toggled with V
        engine::core::Int2 rightDrag; // temporary raw-mouse check
        while (window.PumpMessages()) {
            frameTimer.Tick();
            if (input.WasKeyPressed(VK_ESCAPE)) {
                window.Close();
            }
            if (input.WasKeyPressed('V')) {
                vsync = !vsync;
                LogInfo("VSync {}", vsync ? "on" : "off");
            }

            if (input.WasKeyPressed(VK_F11) || (input.WasKeyPressed(VK_RETURN) && input.IsKeyDown(VK_MENU))) {
                window.ToggleFullscreen();
            }

            // Temporary check of raw mouse input until the camera uses it
            if (input.IsKeyDown(VK_RBUTTON)) {
                rightDrag.x += input.RawMouseDelta().x;
                rightDrag.y += input.RawMouseDelta().y;
            }
            if (input.WasKeyReleased(VK_RBUTTON)) {
                LogInfo("Right-drag: raw mouse moved ({}, {})", rightDrag.x, rightDrag.y);
                rightDrag = {};
            }
            if (frameTimer.FpsUpdated()) {
                const double fps = frameTimer.Fps();
                window.SetTitle(std::format(L"D3D12 Engine - {:.1f} FPS ({:.2f} ms)", fps, 1000.0 / fps));
            }

            // Everything above has read this frame's input; clear the
            // per-frame values (pressed, deltas).
            input.EndFrame();

            // A minimized window has nothing to show.
            // WaitMessage() sleeps until the next message
            if (window.IsMinimized()) {
                ::WaitMessage();
                continue;
            }
            try {
                renderer.BeginFrame(window.Width(), window.Height());
                renderer.Render(frameTimer.TotalSeconds());
                renderer.EndFrame(vsync);
            } catch (const engine::core::HResultError &) {
                // Reset, ResizeBuffers, Present, Signal and others fail with
                // DXGI_ERROR_DEVICE_REMOVED once the GPU is lost
                renderer.LogDeviceRemovedInfo();
                throw;
            }
        }

        return window.ExitCode();
    }

    void LogValidationSummary(const engine::rhi::ValidationStats &validation) {
        if (!validation.active) {
            LogInfo("Validation: not active (Release build or debug layer unavailable)");
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
} // namespace

int WINAPI wWinMain(HINSTANCE /*hInst*/, HINSTANCE /*prevInst*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/) {
    try {
        // Outlives the renderer: its device writes the live-object count when it is destroyed
        engine::rhi::ValidationStats validation;
        const int exitCode = RunSandbox(validation);
        LogValidationSummary(validation);

        LogInfo("Exiting with code {}", exitCode);
        return exitCode;
    } catch (std::exception &e) {
        LogError("{}", e.what());
        MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR);
        return -1;
    }
}
