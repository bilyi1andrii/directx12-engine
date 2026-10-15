#include <cstdint>
#include <format>
#include <directx/d3dx12.h>

#include "core/Log.h"
#include "core/ThrowIfFailed.h"
#include "core/Timer.h"
#include "core/Window.h"
#include "core/Input.h"
#include "core/Assert.h"
#include "rhi/AgilitySdk.h"
#include "rhi/CommandQueue.h"
#include "rhi/Device.h"

using engine::rhi::ComPtr;
using engine::core::LogError;
using engine::core::LogInfo;
using engine::core::ThrowIfFailed;

ENGINE_EXPORT_AGILITY_SDK()

// The number of swap chain back buffers
const uint8_t g_NumFrames = 3;


// DirectX12 Objects
ComPtr<IDXGISwapChain4> g_SwapChain;
ComPtr<ID3D12Resource> g_BackBuffers[g_NumFrames];
ComPtr<ID3D12GraphicsCommandList> g_CommandList;
ComPtr<ID3D12CommandAllocator> g_CommandAllocators[g_NumFrames];
ComPtr<ID3D12DescriptorHeap> g_RTVDescriptorHeap;
UINT g_RTVDescriptorSize;
UINT g_CurrentBackBufferIndex;

// By default, enable V-Sync.
// Can be toggled with the V key.
bool g_VSync = true;
bool g_TearingSupported = false;

uint64_t g_FrameFenceValues[g_NumFrames] = {};


ComPtr<IDXGISwapChain4> CreateSwapChain(IDXGIFactory4* dxgiFactory4, HWND hWnd,
                                        ComPtr<ID3D12CommandQueue> commandQueue,
                                        uint32_t width, uint32_t height, uint32_t bufferCount)
{
    ComPtr<IDXGISwapChain4> dxgiSwapChain4;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc = {1, 0};
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = bufferCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    // It is recommended to always allow tearing if tearing support is available.
    swapChainDesc.Flags = g_TearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(
        commandQueue.Get(),
        hWnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1));

    // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
    // will be handled manually.
    ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));

    return dxgiSwapChain4;
}

ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device2> device,
                                                  D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors)
{
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = numDescriptors;
    desc.Type = type;

    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
}

void UpdateRenderTargetViews(ComPtr<ID3D12Device2> device,
                             ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap)
{
    auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < g_NumFrames; ++i)
    {
        ComPtr<ID3D12Resource> backBuffer;
        ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

        device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

        g_BackBuffers[i] = backBuffer;

        rtvHandle.Offset(rtvDescriptorSize);
    }
}

ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device2> device,
                                                      D3D12_COMMAND_LIST_TYPE type)
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)));

    return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12Device2> device,
                                                    ComPtr<ID3D12CommandAllocator> commandAllocator,
                                                    D3D12_COMMAND_LIST_TYPE type)
{
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ThrowIfFailed(device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

    ThrowIfFailed(commandList->Close());

    return commandList;
}


void Render(engine::rhi::CommandQueue& queue)
{
    ENGINE_ASSERT_MSG(g_CurrentBackBufferIndex < g_NumFrames, "back buffer index {} out of range",
                      g_CurrentBackBufferIndex);
    auto commandAllocator = g_CommandAllocators[g_CurrentBackBufferIndex];
    auto backBuffer = g_BackBuffers[g_CurrentBackBufferIndex];

    ThrowIfFailed(commandAllocator->Reset());
    ThrowIfFailed(g_CommandList->Reset(commandAllocator.Get(), nullptr));

    // Clear
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer.Get(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        g_CommandList->ResourceBarrier(1, &barrier);

        FLOAT clearColor[] = {0.4f, 0.6f, 0.9f, 1.0f};
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(g_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                                          g_CurrentBackBufferIndex, g_RTVDescriptorSize);

        g_CommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    }

    // Present
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        g_CommandList->ResourceBarrier(1, &barrier);

        ThrowIfFailed(g_CommandList->Close());

        queue.ExecuteCommandList(g_CommandList.Get());

        UINT syncInterval = g_VSync ? 1 : 0;
        UINT presentFlags = g_TearingSupported && !g_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
        ThrowIfFailed(g_SwapChain->Present(syncInterval, presentFlags));

        g_FrameFenceValues[g_CurrentBackBufferIndex] = queue.Signal();

        g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

        queue.WaitForFenceValue(g_FrameFenceValues[g_CurrentBackBufferIndex]);
    }
}


int WINAPI wWinMain(HINSTANCE /*hInst*/, HINSTANCE /*prevInst*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/)
{
    try
    {
        engine::core::Timer initTimer;
        initTimer.Start();


        engine::core::Input input;

        engine::core::Window window({
            .title = L"D3D12 Engine", .width = 1280,
            .height = 720
        });

        window.SetCallbacks({
            .onMessage = [&input](const engine::core::WindowMessage& message)
            {
                input.HandleMessage(message);
                return false; // let the window handle it as well
            },
            .onResize = [](uint32_t width, uint32_t height)
            {
                // The swap chain is resized in step 1c; until then the image is stretched.
                LogInfo("Window resized to {}x{}", width, height);
            },
        });

        engine::rhi::Device device;
        g_TearingSupported = device.TearingSupported();

        engine::rhi::CommandQueue queue(device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Direct queue");

        g_SwapChain = CreateSwapChain(device.Factory(), window.Handle(), queue.Native(),
                                      window.Width(), window.Height(),
                                      g_NumFrames);

        g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

        g_RTVDescriptorHeap = CreateDescriptorHeap(device.Native(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, g_NumFrames);
        g_RTVDescriptorSize = device.Native()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        UpdateRenderTargetViews(device.Native(), g_SwapChain, g_RTVDescriptorHeap);

        for (UINT i = 0; i < g_NumFrames; ++i)
        {
            g_CommandAllocators[i] = CreateCommandAllocator(device.Native(), D3D12_COMMAND_LIST_TYPE_DIRECT);
        }
        g_CommandList = CreateCommandList(device.Native(),
                                          g_CommandAllocators[g_CurrentBackBufferIndex],
                                          D3D12_COMMAND_LIST_TYPE_DIRECT);


        LogInfo("Initialized in {:.0f} ms: {}x{}, tearing {}",
                initTimer.ElapsedMs(), window.Width(),
                window.Height(), g_TearingSupported ? "supported" : "not supported");

        // Game loop: handle all pending window messages, update, render
        engine::core::FrameTimer frameTimer;
        engine::core::Int2 rightDrag; // temporary raw-mouse check
        while (window.PumpMessages())
        {
            frameTimer.Tick();
            if (input.WasKeyPressed(VK_ESCAPE))
            {
                window.Close();
            }
            if (input.WasKeyPressed('V'))
            {
                g_VSync = !g_VSync;
                LogInfo("VSync {}", g_VSync ? "on" : "off");
            }

            if (input.WasKeyPressed(VK_F11) || (input.WasKeyPressed(VK_RETURN)
                && input.IsKeyDown(VK_MENU)))
            {
                window.ToggleFullscreen();
            }

            // Temporary check of raw mouse input until the camera uses it
            if (input.IsKeyDown(VK_RBUTTON))
            {
                rightDrag.x += input.RawMouseDelta().x;
                rightDrag.y += input.RawMouseDelta().y;
            }
            if (input.WasKeyReleased(VK_RBUTTON))
            {
                LogInfo("Right-drag: raw mouse moved ({}, {})", rightDrag.x,
                        rightDrag.y);
                rightDrag = {};
            }
            if (frameTimer.FpsUpdated())
            {
                const double fps = frameTimer.Fps();
                window.SetTitle(std::format(L"D3D12 Engine - {:.1f} FPS ({:.2f} ms)", fps, 1000.0 / fps));
            }

            // Everything above has read this frame's input; clear the
            // per-frame values (pressed, deltas).
            input.EndFrame();

            // A minimized window has nothing to show.
            // WaitMessage() sleeps until the next message
            if (window.IsMinimized())
            {
                ::WaitMessage();
                continue;
            }
            try
            {
                Render(queue);
            } catch (const engine::core::HResultError&)
            {
                // Present, Signal and others fail with DXGI_ERROR_DEVICE_REMOVED once the GPU is lost
                device.LogDeviceRemovedInfo();
                throw;
            }
        }

        queue.Flush();

        LogInfo("Exiting with code {}", window.ExitCode());

        return window.ExitCode();
    }
    catch (std::exception& e)
    {
        LogError("{}", e.what());
        MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR);
        return -1;
    }
}
