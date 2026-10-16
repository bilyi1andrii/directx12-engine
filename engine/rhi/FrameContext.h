#ifndef D3D12_ENGINE_FRAMECONTEXT_H
#define D3D12_ENGINE_FRAMECONTEXT_H

#include <cstdint>
#include <string_view>
#include <vector>

#include "rhi/RhiCommon.h"

namespace engine::rhi {

    class CommandQueue;

    // How many frames the CPU may record ahead of the GPU. Frame N uses slot N % kFramesInFlight,
    // so before recording frame N the CPU waits until the GPU has finished frame N - kFramesInFlight.
    // Independent of the swap chain's back-buffer count.
    inline constexpr uint32_t kFramesInFlight = 2;

    // Everything one frame in flight owns that the GPU may still be using after the CPU has moved on:
    // the command allocator (memory of the recorded commands), the fence value that marks the end of
    // the frame's GPU work, and objects whose release must wait for that work to finish.
    //
    // Move-only.

    class FrameContext {
    public:
        FrameContext(NativeDevice *device, D3D12_COMMAND_LIST_TYPE type, std::wstring_view name);

        FrameContext(const FrameContext &) = delete;
        FrameContext &operator=(const FrameContext &) = delete;
        FrameContext(FrameContext &&) noexcept = default;
        FrameContext &operator=(FrameContext &&) noexcept = default;

        // Start of the frame: waits until the GPU has finished this slot's previous frame, then
        // resets the allocator and releases the deferred objects
        void WaitAndReset(CommandQueue &queue);

        // End of the frame: the value returned by CommandQueue::Signal() after submitting its work
        void SetFenceValue(uint64_t fenceValue) noexcept { m_fenceValue = fenceValue; }

        // Keeps a D3D12 object alive until the GPU has finished this frame. Use it instead of
        // releasing anything the frame's command lists may reference.
        void DeferRelease(ComPtr<ID3D12DeviceChild> object);

        [[nodiscard]] ID3D12CommandAllocator *Allocator() const noexcept { return m_allocator.Get(); }
        [[nodiscard]] uint64_t FenceValue() const noexcept { return m_fenceValue; }

    private:
        ComPtr<ID3D12CommandAllocator> m_allocator;
        uint64_t m_fenceValue = 0; // 0: nothing submitted yet, always complete
        std::vector<ComPtr<ID3D12DeviceChild>> m_deferredReleases;
    };

} // namespace engine::rhi

#endif // D3D12_ENGINE_FRAMECONTEXT_H