#ifndef D3D12_ENGINE_COMMANDCONTEXT_H
#define D3D12_ENGINE_COMMANDCONTEXT_H

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "rhi/RhiCommon.h"

namespace engine::rhi
{
    // Thin wrapper over a graphics command list. Collects resource barriers and submits them as one
    // ResourceBarrier() call right before the next command that needs them (or at Close()).
    //
    // Usage per frame: Begin(frame.Allocator()) -> Transition/Clear/... -> Close() -> CommandQueue::ExecuteCommandList(Native()).
    // Calls made directly on Native() bypass the batching: call FlushBarriers() before them.
    //
    // Not copyable or movable.
    class CommandContext
    {
    public:
        CommandContext(NativeDevice *device, D3D12_COMMAND_LIST_TYPE type, std::wstring_view name);

        CommandContext(const CommandContext &) = delete;
        CommandContext &operator=(const CommandContext &) = delete;
        CommandContext(CommandContext &&) = delete;
        CommandContext &operator=(CommandContext &&) = delete;

        // Starts recording into memory from `allocator`. The allocator must not be in use by the GPU.
        void Begin(ID3D12CommandAllocator *allocator);

        // Queues a state transition. `before` must be the resource's current state.
        void Transition(ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after,
                        UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
        // Records all queued barriers
        void FlushBarriers();

        void ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, std::span<const float, 4> color);

        // Flushes barriers and ends recording; the list can then be executed
        void Close();

        [[nodiscard]] ID3D12GraphicsCommandList *Native() const noexcept { return m_list.Get(); }

    private:
        static constexpr uint32_t kMaxPendingBarriers = 16;

        ComPtr<ID3D12GraphicsCommandList> m_list;
        std::array<D3D12_RESOURCE_BARRIER, kMaxPendingBarriers> m_pendingBarriers{};
        uint32_t m_pendingBarrierCount = 0;
        bool m_recording = false;
    };
} // namespace engine::rhi

#endif // D3D12_ENGINE_COMMANDCONTEXT_H