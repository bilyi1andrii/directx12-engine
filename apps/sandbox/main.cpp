#include <array>
#include <cstdint>
#include <format>
#include <vector>

#include "core/Log.h"
#include "core/ThrowIfFailed.h"
#include "core/Timer.h"
#include "core/Window.h"
#include "core/Input.h"
#include "core/Assert.h"
#include "rhi/AgilitySdk.h"
#include "rhi/CommandContext.h"
#include "rhi/CommandQueue.h"
#include "rhi/DescriptorAllocator.h"
#include "rhi/Device.h"
#include "rhi/FrameContext.h"
#include "rhi/SwapChain.h"

using engine::core::LogError;
using engine::core::LogInfo;

ENGINE_EXPORT_AGILITY_SDK()

void Render(engine::rhi::CommandQueue &queue, engine::rhi::FrameContext &frame, engine::rhi::CommandContext &context,
            engine::rhi::SwapChain &swapChain, bool vsync) {
    // Begin frame: this slot was last used kFramesInFlight frames ago; wait until
    // the GPU has finished that frame before reusing its allocator
    frame.WaitAndReset(queue);
    context.Begin(frame.Allocator());

    ID3D12Resource *backBuffer = swapChain.CurrentBackBuffer();

    // Record
    constexpr std::array clearColor{0.4f, 0.6f, 0.9f, 1.0f};
    context.Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    context.ClearRenderTarget(swapChain.CurrentRtv(), clearColor);
    context.Transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    context.Close();

    // End frame: submit, present, and remember which fence value marks the end of
    // this frame's GPU work
    queue.ExecuteCommandList(context.Native());
    swapChain.Present(vsync);
    frame.SetFenceValue(queue.Signal());
}

int WINAPI wWinMain(HINSTANCE /*hInst*/, HINSTANCE /*prevInst*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/) {
    try {
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

        engine::rhi::Device device;

        // Declared before the queue, so they are destroyed after it: the queue's
        // destructor waits for the GPU, and only then are the allocators and the
        // command list released
        std::vector<engine::rhi::FrameContext> frames;
        frames.reserve(engine::rhi::kFramesInFlight);
        for (uint32_t i = 0; i < engine::rhi::kFramesInFlight; ++i) {
            frames.emplace_back(device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, std::format(L"Frame {}", i));
        }
        engine::rhi::CommandContext context(device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Main command list");

        engine::rhi::CommandQueue queue(device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Direct queue");

        // Back buffers plus the render targets of later steps; 64 slots cost a few KB of CPU memory
        engine::rhi::DescriptorAllocator rtvAllocator(device.Native(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 64, L"RTV heap");
        // Declared after the queue and the allocator: its destructor flushes the queue and returns
        // its RTVs, so both must still exist
        engine::rhi::SwapChain swapChain(device, queue, rtvAllocator, window.Handle(), window.Width(), window.Height());

        LogInfo("Initialized in {:.0f} ms: {}x{}, tearing {}", initTimer.ElapsedMs(), window.Width(), window.Height(),
                device.TearingSupported() ? "supported" : "not supported");

        // Game loop: handle all pending window messages, update, render
        engine::core::FrameTimer frameTimer;
        uint64_t frameNumber = 0;
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
                // Match the window's current client size (no-op when unchanged)
                swapChain.Resize(window.Width(), window.Height());
                Render(queue, frames[frameNumber % engine::rhi::kFramesInFlight], context, swapChain, vsync);
                ++frameNumber;
            } catch (const engine::core::HResultError &) {
                // Present, Signal and others fail with DXGI_ERROR_DEVICE_REMOVED once
                // the GPU is lost
                device.LogDeviceRemovedInfo();
                throw;
            }
        }

        queue.Flush();

        LogInfo("Exiting with code {}", window.ExitCode());

        return window.ExitCode();
    } catch (std::exception &e) {
        LogError("{}", e.what());
        MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR);
        return -1;
    }
}
