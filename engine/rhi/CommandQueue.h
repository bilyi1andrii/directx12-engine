#ifndef D3D12_ENGINE_COMMANDQUEUE_H
#define D3D12_ENGINE_COMMANDQUEUE_H

// A queue the GPU runs command lists from, with a fence that tells the CPU how far the GPU has got, so memory the
// GPU used can be reused or freed.

#include <cstdint>
#include <string_view>

#include "core/UniqueHandle.h"
#include "rhi/RhiCommon.h"

namespace rhi {
    class CommandQueue {
    public:
        CommandQueue(NativeDevice *device, D3D12_COMMAND_LIST_TYPE type, std::wstring_view name);
        ~CommandQueue();

        CommandQueue(const CommandQueue &) = delete;
        CommandQueue &operator=(const CommandQueue &) = delete;
        CommandQueue(CommandQueue &&) = delete;
        CommandQueue &operator=(CommandQueue &&) = delete;

        void ExecuteCommandList(ID3D12CommandList *commandList);

        // adds a command to the queue to increment the fence value
        [[nodiscard]] uint64_t Signal();

        [[nodiscard]] bool IsFenceComplete(uint64_t fenceValue) const;

        void WaitForFenceValue(uint64_t fenceValue);

        // waits until the GPU is idle: on resize, shutdown and upload completion
        void Flush();

        [[nodiscard]] uint64_t TimestampFrequency() const;

        [[nodiscard]] ID3D12CommandQueue *Native() const noexcept { return m_queue.Get(); }

    private:
        ComPtr<ID3D12CommandQueue> m_queue;
        ComPtr<ID3D12Fence> m_fence;
        uint64_t m_lastSignaledValue = 0;
        core::UniqueHandle m_fenceEvent;
    };
} // namespace rhi

#endif //D3D12_ENGINE_COMMANDQUEUE_H
