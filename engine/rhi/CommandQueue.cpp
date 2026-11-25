#include "rhi/CommandQueue.h"

#include <exception>
#include <iterator>
#include <string>

#include "core/Log.h"
#include "core/ThrowIfFailed.h"

namespace rhi {
    using core::ThrowIfFailed;

    CommandQueue::CommandQueue(NativeDevice *device, D3D12_COMMAND_LIST_TYPE type, std::wstring_view name) {
        const D3D12_COMMAND_QUEUE_DESC desc{
            .Type = type,
            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 0,
        };
        ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_queue)));
        ThrowIfFailed(m_queue->SetName(std::wstring(name).c_str()));

        ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        ThrowIfFailed(m_fence->SetName((std::wstring(name) + L" fence").c_str()));

        m_fenceEvent.reset(::CreateEventW(nullptr, FALSE, FALSE, nullptr));
        core::ThrowIfWin32Failed(m_fenceEvent != nullptr);
    }

    CommandQueue::~CommandQueue() {
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
        return m_fence->GetCompletedValue() >= fenceValue;
    }

    void CommandQueue::WaitForFenceValue(uint64_t fenceValue) {
        if (IsFenceComplete(fenceValue)) {
            return;
        }
        ThrowIfFailed(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent.get()));
        core::ThrowIfWin32Failed(::WaitForSingleObject(m_fenceEvent.get(), INFINITE) == WAIT_OBJECT_0);
    }

    void CommandQueue::Flush() {
        WaitForFenceValue(Signal());
    }

    uint64_t CommandQueue::TimestampFrequency() const {
        uint64_t ticksPerSecond = 0;
        ThrowIfFailed(m_queue->GetTimestampFrequency(&ticksPerSecond));
        return ticksPerSecond;
    }
} // namespace rhi
