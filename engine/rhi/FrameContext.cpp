#include "rhi/FrameContext.h"

#include <string>
#include <utility>

#include "core/ThrowIfFailed.h"
#include "rhi/CommandQueue.h"

namespace engine::rhi {
    using core::ThrowIfFailed;

    FrameContext::FrameContext(NativeDevice *device, D3D12_COMMAND_LIST_TYPE type, std::wstring_view name) {
        // A command allocator owns the memory that recorded commands are written into. The GPU reads
        // that memory while executing, so the allocator may only be Reset() once that work is done.
        ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&m_allocator)));
        ThrowIfFailed(m_allocator->SetName((std::wstring(name) + L" allocator").c_str()));
    }

    void FrameContext::WaitAndReset(CommandQueue &queue) {
        queue.WaitForFenceValue(m_fenceValue);

        // The GPU is done with everything this slot recorded last time
        ThrowIfFailed(m_allocator->Reset());
        m_deferredReleases.clear();
    }

    void FrameContext::DeferRelease(Allocation allocation) {
        m_deferredReleases.push_back(std::move(allocation));
    }
} // namespace engine::rhi