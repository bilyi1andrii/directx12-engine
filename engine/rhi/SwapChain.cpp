#include "rhi/SwapChain.h"

#include <exception>
#include <format>

#include "core/Assert.h"
#include "core/Log.h"
#include "core/ThrowIfFailed.h"
#include "rhi/CommandQueue.h"
#include "rhi/Device.h"

namespace engine::rhi {
    using core::ThrowIfFailed;

    SwapChain::SwapChain(const Device &device, CommandQueue &queue, DescriptorAllocator &rtvAllocator, HWND hwnd,
                         uint32_t width, uint32_t height)
        : m_device(device.Native()), m_queue(queue), m_rtvAllocator(rtvAllocator), m_width(width), m_height(height),
          m_tearingSupported(device.TearingSupported()),
          m_flags(device.TearingSupported() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0) {
        // Flip model: the swap chain's images are handed to the Windows compositor directly instead of
        // being copied, which is faster and required for tearing and variable refresh rate.
        // FLIP_DISCARD: the contents of a back buffer are undefined after Present, so every frame must
        // redraw all of it (we clear it first).
        const DXGI_SWAP_CHAIN_DESC1 desc{
            .Width = width,
            .Height = height,
            .Format = kFormat,
            .Stereo = FALSE,
            .SampleDesc = {.Count = 1, .Quality = 0}, // flip model swap chains cannot be multisampled
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = kBackBufferCount,
            .Scaling = DXGI_SCALING_STRETCH,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
            .Flags = m_flags,
        };

        // In D3D12 the swap chain is created on a command queue (not the device): Present is queued
        // behind the work already submitted to it
        ComPtr<IDXGISwapChain1> swapChain1;
        ThrowIfFailed(
            device.Factory()->CreateSwapChainForHwnd(queue.Native(), hwnd, &desc, nullptr, nullptr, &swapChain1));
        // The app owns Alt+Enter (borderless fullscreen), so DXGI must not switch to exclusive fullscreen
        ThrowIfFailed(device.Factory()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
        // IDXGISwapChain3+ has GetCurrentBackBufferIndex
        ThrowIfFailed(swapChain1.As(&m_swapChain));

        CreateBackBufferViews();
    }

    SwapChain::~SwapChain() {
        // The GPU may still be rendering into or presenting a back buffer
        try {
            m_queue.Flush();
        } catch (const std::exception &e) {
            core::LogError("SwapChain: flush at destruction failed: {}", e.what());
        }
    }

    void SwapChain::Present(bool vsync) {
        // Sync interval 1: show the image at the next vblank (blocks when the queue of images is full).
        // 0 + ALLOW_TEARING: show it immediately, even halfway through the screen refresh (visible tear line)
        const UINT syncInterval = vsync ? 1 : 0;
        const UINT flags = !vsync && m_tearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0;
        ThrowIfFailed(m_swapChain->Present(syncInterval, flags));
    }

    bool SwapChain::Resize(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0 || (width == m_width && height == m_height)) {
            return false;
        }

        // ResizeBuffers fails while anything still references a back buffer, and the GPU may still be
        // using them. Flushing is fine here: resizing is rare (one of the allowed full flushes).
        m_queue.Flush();
        ReleaseBackBuffers();

        // 0 count and UNKNOWN format keep the current ones; the flags must match the creation flags
        ThrowIfFailed(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, m_flags));
        m_width = width;
        m_height = height;

        CreateBackBufferViews();
        core::LogInfo("Swap chain resized to {}x{}", width, height);
        return true;
    }

    ID3D12Resource *SwapChain::CurrentBackBuffer() const {
        // DXGI decides which image is next; it is not simply frame number % kBackBufferCount
        const UINT index = m_swapChain->GetCurrentBackBufferIndex();
        ENGINE_ASSERT_MSG(index < kBackBufferCount, "back buffer index {} out of range", index);
        return m_backBuffers[index].Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::CurrentRtv() const {
        const UINT index = m_swapChain->GetCurrentBackBufferIndex();
        ENGINE_ASSERT_MSG(index < kBackBufferCount, "back buffer index {} out of range", index);
        return m_rtvs[index].Cpu();
    }

    void SwapChain::CreateBackBufferViews() {
        // An RTV describes how the pipeline writes to the texture. This one views the UNORM buffer as sRGB.
        const D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{
            .Format = kRtvFormat,
            .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
            .Texture2D = {.MipSlice = 0, .PlaneSlice = 0},
        };
        for (UINT i = 0; i < kBackBufferCount; ++i) {
            ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])));
            ThrowIfFailed(m_backBuffers[i]->SetName(std::format(L"Back buffer {}", i).c_str()));

            m_rtvs[i] = m_rtvAllocator.Allocate();
            m_device->CreateRenderTargetView(m_backBuffers[i].Get(), &rtvDesc, m_rtvs[i].Cpu());
        }
    }

    void SwapChain::ReleaseBackBuffers() noexcept {
        for (UINT i = 0; i < kBackBufferCount; ++i) {
            m_rtvs[i].Reset();
            m_backBuffers[i].Reset();
        }
    }
} // namespace engine::rhi
