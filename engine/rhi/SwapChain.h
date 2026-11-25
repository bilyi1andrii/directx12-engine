#ifndef D3D12_ENGINE_SWAPCHAIN_H
#define D3D12_ENGINE_SWAPCHAIN_H

// The window's back buffers: each frame is drawn into one and shown with Present, with tearing when vsync is off.
// The scene draws through an sRGB view of the buffer, the UI through a plain one.

#include <array>
#include <cstdint>

#include "rhi/DescriptorAllocator.h"
#include "rhi/RhiCommon.h"

namespace rhi {
    class CommandQueue;
    class Device;

    inline constexpr uint32_t kBackBufferCount = 3;

    class SwapChain {
    public:
        static constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        static constexpr DXGI_FORMAT kRtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        SwapChain(const Device &device, CommandQueue &queue, DescriptorAllocator &rtvAllocator, HWND hwnd,
                  uint32_t width, uint32_t height);
        ~SwapChain();

        SwapChain(const SwapChain &) = delete;
        SwapChain &operator=(const SwapChain &) = delete;
        SwapChain(SwapChain &&) = delete;
        SwapChain &operator=(SwapChain &&) = delete;

        void Present(bool vsync);

        void Resize(uint32_t width, uint32_t height);

        [[nodiscard]] ID3D12Resource *CurrentBackBuffer() const;
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CurrentRtv() const;
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CurrentUiRtv() const;

        [[nodiscard]] uint32_t Width() const noexcept { return m_width; }
        [[nodiscard]] uint32_t Height() const noexcept { return m_height; }

    private:
        void CreateBackBufferViews();
        void ReleaseBackBuffers() noexcept;

        NativeDevice *m_device;
        CommandQueue &m_queue;
        DescriptorAllocator &m_rtvAllocator;
        ComPtr<IDXGISwapChain4> m_swapChain;
        std::array<ComPtr<ID3D12Resource>, kBackBufferCount> m_backBuffers;
        std::array<Descriptor, kBackBufferCount> m_rtvs;   // kRtvFormat
        std::array<Descriptor, kBackBufferCount> m_uiRtvs; // kFormat
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        bool m_tearingSupported = false;
        UINT m_flags = 0;
    };
} // namespace rhi

#endif // D3D12_ENGINE_SWAPCHAIN_H
