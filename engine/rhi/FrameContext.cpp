#include "rhi/FrameContext.h"

#include <string>
#include <utility>

#include "core/ThrowIfFailed.h"
#include "rhi/BindlessHeap.h"
#include "rhi/CommandQueue.h"

namespace rhi {
    using core::ThrowIfFailed;

    FrameContext::FrameContext(NativeDevice *device, rhi::Allocator &allocator, D3D12_COMMAND_LIST_TYPE type,
                               std::wstring_view name)
        : m_uploadRing(allocator, std::wstring(name) + L" upload") {
        // A command allocator owns the memory that recorded commands are written into. The GPU reads
        // that memory while executing, so the allocator may only be Reset() once that work is done.
        ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&m_commandAllocator)));
        ThrowIfFailed(m_commandAllocator->SetName((std::wstring(name) + L" command allocator").c_str()));
    }

    FrameContext::~FrameContext() {
        FreeDeferredIndices();
    }

    void FrameContext::WaitAndReset(CommandQueue &queue) {
        queue.WaitForFenceValue(m_fenceValue);

        // The GPU is done with everything this slot recorded last time
        ThrowIfFailed(m_commandAllocator->Reset());
        m_uploadRing.Reset();
        m_deferredReleases.clear();
        FreeDeferredIndices();
    }

    void FrameContext::DeferRelease(Allocation allocation) {
        m_deferredReleases.push_back(std::move(allocation));
    }

    void FrameContext::DeferFree(BindlessHeap &heap, uint32_t index) {
        m_deferredIndices.push_back({.heap = &heap, .index = index});
    }

    void FrameContext::FreeDeferredIndices() noexcept {
        for (const DeferredIndex &deferred : m_deferredIndices) {
            deferred.heap->Free(deferred.index);
        }
        m_deferredIndices.clear();
    }
} // namespace rhi
