#include "rhi/CommandContext.h"

#include <string>

#include "core/Assert.h"
#include "core/ThrowIfFailed.h"

namespace engine::rhi {
    using core::ThrowIfFailed;

    CommandContext::CommandContext(NativeDevice *device, D3D12_COMMAND_LIST_TYPE type, std::wstring_view name) {
        // CreateCommandList1 (ID3D12Device4) creates the list closed and without an allocator,
        // so it needs no Close() here; Begin() attaches the frame's allocator.
        ThrowIfFailed(device->CreateCommandList1(0, type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_list)));
        ThrowIfFailed(m_list->SetName(std::wstring(name).c_str()));
    }

    void CommandContext::Begin(ID3D12CommandAllocator *allocator) {
        ENGINE_ASSERT_MSG(!m_recording, "CommandContext::Begin called while already recording");
        // Unlike the allocator, the list itself can be reset as soon as it has been submitted:
        // the recorded commands live in the allocator's memory, not in the list
        ThrowIfFailed(m_list->Reset(allocator, nullptr));
        m_recording = true;
    }

    void CommandContext::Transition(ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after,
                                    UINT subresource) {
        ENGINE_ASSERT(m_recording);
        ENGINE_ASSERT_MSG(before != after, "Transition with identical before/after state {}", static_cast<int>(before));

        if (m_pendingBarrierCount == kMaxPendingBarriers) {
            FlushBarriers();
        }
        // A transition barrier tells the GPU that the resource changes use (e.g. presented image ->
        // render target). The GPU may need to finish earlier work, flush caches or change the memory
        // layout (compression) before the next use, so every change of use needs one.
        m_pendingBarriers[m_pendingBarrierCount++] = {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition =
                {
                    .pResource = resource,
                    .Subresource = subresource,
                    .StateBefore = before,
                    .StateAfter = after,
                },
        };
    }

    void CommandContext::FlushBarriers() {
        // One call with several barriers lets the driver combine their waits and cache flushes
        if (m_pendingBarrierCount > 0) {
            m_list->ResourceBarrier(m_pendingBarrierCount, m_pendingBarriers.data());
            m_pendingBarrierCount = 0;
        }
    }

    void CommandContext::ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, std::span<const float, 4> color) {
        ENGINE_ASSERT(m_recording);
        FlushBarriers();
        m_list->ClearRenderTargetView(rtv, color.data(), 0, nullptr);
    }

    void CommandContext::ClearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth) {
        ENGINE_ASSERT(m_recording);
        FlushBarriers();
        m_list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
    }

    void CommandContext::CopyBuffer(ID3D12Resource *dst, uint64_t dstOffset, ID3D12Resource *src, uint64_t srcOffset,
                                    uint64_t size) {
        ENGINE_ASSERT(m_recording);
        FlushBarriers();
        m_list->CopyBufferRegion(dst, dstOffset, src, srcOffset, size);
    }

    void CommandContext::Close() {
        ENGINE_ASSERT_MSG(m_recording, "CommandContext::Close called without Begin");
        FlushBarriers();
        ThrowIfFailed(m_list->Close());
        m_recording = false;
    }
} // namespace engine::rhi