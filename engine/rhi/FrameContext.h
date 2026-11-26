#ifndef D3D12_ENGINE_FRAMECONTEXT_H
#define D3D12_ENGINE_FRAMECONTEXT_H

#include <cstdint>
#include <string_view>
#include <vector>

#include "rhi/Allocator.h"
#include "rhi/RhiCommon.h"
#include "rhi/UploadRing.h"

namespace rhi {

    class BindlessHeap;
    class CommandQueue;

    // How many frames the CPU may record ahead of the GPU. Frame N uses slot N % kFramesInFlight,
    // so before recording frame N the CPU waits until the GPU has finished frame N - kFramesInFlight.
    // Independent of the swap chain's back-buffer count.
    inline constexpr uint32_t kFramesInFlight = 2;

    // Everything one frame in flight owns that the GPU may still be using after the CPU has moved on:
    // the command allocator (memory of the recorded commands), the upload ring (constants and instance
    // data written this frame), the fence value that marks the end of the frame's GPU work, and
    // resources and bindless indices whose release must wait for that work to finish.
    //
    // Move-constructible only. The resource allocator and every bindless heap passed to DeferFree must outlive it.

    class FrameContext {
    public:
        FrameContext(NativeDevice *device, rhi::Allocator &allocator, D3D12_COMMAND_LIST_TYPE type,
                     std::wstring_view name);
        ~FrameContext();

        FrameContext(const FrameContext &) = delete;
        FrameContext &operator=(const FrameContext &) = delete;
        FrameContext(FrameContext &&) noexcept = default;
        // Assigning would drop the target's pending bindless indices without freeing them
        FrameContext &operator=(FrameContext &&) = delete;

        // Start of the frame: waits until the GPU has finished this slot's previous frame, then
        // resets the command allocator and the upload ring and releases the deferred resources and indices
        void WaitAndReset(CommandQueue &queue);

        // End of the frame: the value returned by CommandQueue::Signal() after submitting its work
        void SetFenceValue(uint64_t fenceValue) noexcept { m_fenceValue = fenceValue; }

        // Keeps the resource and its memory alive until the GPU finishes this frame.
        // Use this instead of immediate release for any resource used by this frame's commands.
        // Otherwise, D3D12MA might reuse the memory while the GPU is still reading it.
        void DeferRelease(Allocation allocation);

        // Returns a bindless index to `heap` once the GPU has finished this frame. Shaders of this frame may
        // still read the descriptor in that slot; reusing it earlier would hand them another resource's view.
        void DeferFree(BindlessHeap &heap, uint32_t index);

        [[nodiscard]] ID3D12CommandAllocator *CommandAllocator() const noexcept { return m_commandAllocator.Get(); }
        // Per-frame upload memory: allocations are valid until this slot's next WaitAndReset()
        [[nodiscard]] UploadRing &Ring() noexcept { return m_uploadRing; }

    private:
        struct DeferredIndex {
            BindlessHeap *heap;
            uint32_t index;
        };

        void FreeDeferredIndices() noexcept;

        ComPtr<ID3D12CommandAllocator> m_commandAllocator;
        UploadRing m_uploadRing;
        uint64_t m_fenceValue = 0; // 0: nothing submitted yet, always complete
        std::vector<Allocation> m_deferredReleases;
        std::vector<DeferredIndex> m_deferredIndices;
    };

} // namespace rhi

#endif // D3D12_ENGINE_FRAMECONTEXT_H