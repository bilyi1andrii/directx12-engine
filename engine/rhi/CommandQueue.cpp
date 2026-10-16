#include "rhi/CommandQueue.h"

#include <exception>
#include <iterator>
#include <string>

#include "core/Log.h"
#include "core/ThrowIfFailed.h"

namespace engine::rhi {
    using core::ThrowIfFailed;

    CommandQueue::CommandQueue(NativeDevice *device, D3D12_COMMAND_LIST_TYPE type, std::wstring_view name) {
        // The GPU executes command lists from queues. DIRECT queues accept everything (draw, compute,
        // copy); COMPUTE and COPY queues accept less but can run in parallel with the direct one.
        const D3D12_COMMAND_QUEUE_DESC desc{
            .Type = type,
            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 0,
        };
        ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_queue)));
        ThrowIfFailed(m_queue->SetName(std::wstring(name).c_str()));

        // A fence is a 64-bit value in GPU-visible memory. The queue writes to it (Signal),
        // the CPU reads it (GetCompletedValue) or asks to be woken up when it reaches a value.
        ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        ThrowIfFailed(m_fence->SetName((std::wstring(name) + L" fence").c_str()));

        // Auto-reset event: after one wait wakes up, it is unsignaled again
        m_fenceEvent.reset(::CreateEventW(nullptr, FALSE, FALSE, nullptr));
        core::ThrowIfWin32Failed(m_fenceEvent != nullptr);
    }

    CommandQueue::~CommandQueue() {
        // Releasing the queue, or anything its pending command lists use, while the GPU is still
        // running them is an error. Waiting here makes destruction safe even when unwinding from
        // an exception.
        try {
            Flush();
        } catch (const std::exception &e) {
            core::LogError("CommandQueue flush at destruction failed: {}", e.what());
        }
    }

    void CommandQueue::ExecuteCommandList(ID3D12CommandList *commandList) {
        ID3D12CommandList *const lists[] = {commandList};
        m_queue->ExecuteCommandLists(static_cast<UINT>(std::size(lists)), lists);
    }

    uint64_t CommandQueue::Signal() {
        ++m_lastSignaledValue;
        ThrowIfFailed(m_queue->Signal(m_fence.Get(), m_lastSignaledValue));
        return m_lastSignaledValue;
    }

    bool CommandQueue::IsFenceComplete(uint64_t fenceValue) const {
        // After device removal the fence reports UINT64_MAX, so every value counts as complete
        // and nothing waits forever
        return m_fence->GetCompletedValue() >= fenceValue;
    }

    void CommandQueue::WaitForFenceValue(uint64_t fenceValue) {
        if (IsFenceComplete(fenceValue)) {
            return;
        }
        // Ask the fence to signal the event once it reaches fenceValue, then sleep until it does.
        // Sleeping in the kernel uses no CPU, unlike polling GetCompletedValue() in a loop.
        ThrowIfFailed(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent.get()));
        core::ThrowIfWin32Failed(::WaitForSingleObject(m_fenceEvent.get(), INFINITE) == WAIT_OBJECT_0);
    }

    void CommandQueue::Flush() {
        WaitForFenceValue(Signal());
    }
} // namespace engine::rhi
