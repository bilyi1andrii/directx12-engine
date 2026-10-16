#ifndef D3D12_ENGINE_SWAPCHAIN_H
#define D3D12_ENGINE_SWAPCHAIN_H

#include <array>
#include <cstdint>

#include "rhi/DescriptorAllocator.h"
#include "rhi/RhiCommon.h"

namespace engine::rhi {
    class CommandQueue;
    class Device;

    // Number of swap chain images. Independent of kFramesInFlight: with 3 images the GPU can render
    // the next frame while one image is on screen and another waits for vblank.
    inline constexpr uint32_t kBackBufferCount = 3;

    // The window's swap chain: the back buffers the frame is rendered into and Present() that hands
    // them to Windows for display. Flip model (DXGI_SWAP_EFFECT_FLIP_DISCARD), 8-bit UNORM, tearing
    // allowed when supported (vsync off without waiting for vblank).
    //
    // Not copyable or movable. The queue and the RTV allocator must outlive it.
    class SwapChain {
    public:
        static constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

        SwapChain(const Device &device, CommandQueue &queue, DescriptorAllocator &rtvAllocator, HWND hwnd,
                  uint32_t width, uint32_t height);
        // Waits for the GPU before releasing the back buffers
        ~SwapChain();

        SwapChain(const SwapChain &) = delete;
        SwapChain &operator=(const SwapChain &) = delete;
        SwapChain(SwapChain &&) = delete;
        SwapChain &operator=(SwapChain &&) = delete;

        // Shows the current back buffer. vsync: wait for the next vblank; otherwise present at once
        // (with tearing if supported)
        void Present(bool vsync);

        // Recreates the back buffers at the new size. Waits until the GPU is idle first.
        // Does nothing for an unchanged size or 0x0 (minimized), so it is cheap to call every frame.
        // Returns true if the buffers were recreated (size-dependent targets must follow)
        bool Resize(uint32_t width, uint32_t height);

        // The back buffer this frame renders into. Changes after every Present()
        [[nodiscard]] ID3D12Resource *CurrentBackBuffer() const;
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CurrentRtv() const;

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
        std::array<Descriptor, kBackBufferCount> m_rtvs;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        bool m_tearingSupported = false;
        UINT m_flags = 0; // creation flags; ResizeBuffers must get the same ones
    };
} // namespace engine::rhi

#endif // D3D12_ENGINE_SWAPCHAIN_H
