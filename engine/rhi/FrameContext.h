#ifndef D3D12_ENGINE_FRAMECONTEXT_H
#define D3D12_ENGINE_FRAMECONTEXT_H

// What one frame in flight owns while the GPU may still use it: its command memory, its upload memory, and the
// resources and bindless slots waiting to be freed once the GPU has finished the frame.

#include <cstdint>
#include <string_view>
#include <vector>

#include "rhi/Allocator.h"
#include "rhi/RhiCommon.h"
#include "rhi/UploadRing.h"

namespace rhi {

    class BindlessHeap;
    class CommandQueue;

    // the CPU records at most one frame ahead of the GPU
    inline constexpr uint32_t kFramesInFlight = 2;

    class FrameContext {
    public:
        FrameContext(NativeDevice *device, rhi::Allocator &allocator, D3D12_COMMAND_LIST_TYPE type,
                     std::wstring_view name);
        ~FrameContext();

        FrameContext(const FrameContext &) = delete;
        FrameContext &operator=(const FrameContext &) = delete;
        FrameContext(FrameContext &&) noexcept = default;
        FrameContext &operator=(FrameContext &&) = delete;

        void WaitAndReset(CommandQueue &queue);

        void SetFenceValue(uint64_t fenceValue) noexcept { m_fenceValue = fenceValue; }

        void DeferRelease(Allocation allocation);

        void DeferFree(BindlessHeap &heap, uint32_t index);

        [[nodiscard]] ID3D12CommandAllocator *CommandAllocator() const noexcept { return m_commandAllocator.Get(); }
        [[nodiscard]] UploadRing &Ring() noexcept { return m_uploadRing; }

    private:
        struct DeferredIndex {
            BindlessHeap *heap;
            uint32_t index;
        };

        void FreeDeferredIndices() noexcept;

        ComPtr<ID3D12CommandAllocator> m_commandAllocator;
        UploadRing m_uploadRing;
        uint64_t m_fenceValue = 0; // set after the frame is submitted
        std::vector<Allocation> m_deferredReleases;
        std::vector<DeferredIndex> m_deferredIndices;
    };

} // namespace rhi

#endif // D3D12_ENGINE_FRAMECONTEXT_H
