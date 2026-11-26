#include "rhi/SwapChain.h"

#include <exception>
#include <format>

#include "core/Log.h"
#include "core/ThrowIfFailed.h"
#include "rhi/CommandQueue.h"
#include "rhi/Device.h"

namespace rhi {
    using core::ThrowIfFailed;

    SwapChain::SwapChain(const Device &device, CommandQueue &queue, DescriptorAllocator &rtvAllocator, HWND hwnd,
                         uint32_t width, uint32_t height)
        : m_device(device.Native()), m_queue(queue), m_rtvAllocator(rtvAllocator), m_width(width), m_height(height),
          m_tearingSupported(device.TearingSupported()),
          m_flags(device.TearingSupported() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0) {
        const DXGI_SWAP_CHAIN_DESC1 desc{
            .Width = width,
            .Height = height,
            .Format = kFormat,
            .Stereo = FALSE,
            .SampleDesc = {.Count = 1, .Quality = 0},
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = kBackBufferCount,
            .Scaling = DXGI_SCALING_STRETCH,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
            .Flags = m_flags,
        };

        ComPtr<IDXGISwapChain1> swapChain1;
        ThrowIfFailed(
            device.Factory()->CreateSwapChainForHwnd(queue.Native(), hwnd, &desc, nullptr, nullptr, &swapChain1));
        ThrowIfFailed(device.Factory()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
        ThrowIfFailed(swapChain1.As(&m_swapChain));

        CreateBackBufferViews();
    }

    SwapChain::~SwapChain() {
        try {
            m_queue.Flush();
        } catch (const std::exception &e) {
            core::LogError("SwapChain: flush at destruction failed: {}", e.what());
        }
    }

    void SwapChain::Present(bool vsync) {
        const UINT syncInterval = vsync ? 1 : 0;
        const UINT flags = !vsync && m_tearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0;
        ThrowIfFailed(m_swapChain->Present(syncInterval, flags));
    }

    void SwapChain::Resize(uint32_t width, uint32_t height) {
        if (width == m_width && height == m_height) {
            return;
        }

        m_queue.Flush();
        ReleaseBackBuffers();

        ThrowIfFailed(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, m_flags));
        m_width = width;
        m_height = height;

        CreateBackBufferViews();
        core::LogInfo("Swap chain resized to {}x{}", width, height);
    }

    ID3D12Resource *SwapChain::CurrentBackBuffer() const {
        return m_backBuffers[m_swapChain->GetCurrentBackBufferIndex()].Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::CurrentRtv() const {
        return m_rtvs[m_swapChain->GetCurrentBackBufferIndex()].Cpu();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::CurrentUiRtv() const {
        return m_uiRtvs[m_swapChain->GetCurrentBackBufferIndex()].Cpu();
    }

    void SwapChain::CreateBackBufferViews() {
        const D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{
            .Format = kRtvFormat,
            .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
            .Texture2D = {.MipSlice = 0, .PlaneSlice = 0},
        };
        const D3D12_RENDER_TARGET_VIEW_DESC uiRtvDesc{
            .Format = kFormat,
            .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
            .Texture2D = {.MipSlice = 0, .PlaneSlice = 0},
        };
        for (UINT i = 0; i < kBackBufferCount; ++i) {
            ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])));
            ThrowIfFailed(m_backBuffers[i]->SetName(std::format(L"Back buffer {}", i).c_str()));

            m_rtvs[i] = m_rtvAllocator.Allocate();
            m_device->CreateRenderTargetView(m_backBuffers[i].Get(), &rtvDesc, m_rtvs[i].Cpu());
            m_uiRtvs[i] = m_rtvAllocator.Allocate();
            m_device->CreateRenderTargetView(m_backBuffers[i].Get(), &uiRtvDesc, m_uiRtvs[i].Cpu());
        }
    }

    void SwapChain::ReleaseBackBuffers() noexcept {
        for (UINT i = 0; i < kBackBufferCount; ++i) {
            m_rtvs[i].Reset();
            m_uiRtvs[i].Reset();
            m_backBuffers[i].Reset();
        }
    }
} // namespace rhi
