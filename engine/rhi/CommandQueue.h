#ifndef D3D12_ENGINE_COMMANDQUEUE_H
#define D3D12_ENGINE_COMMANDQUEUE_H

#include <cstdint>
#include <string_view>

#include "core/UniqueHandle.h"
#include "rhi/RhiCommon.h"

namespace rhi {
    // A GPU command queue plus the fence that tracks how far the GPU has got through it.
    //
    // The CPU submits command lists and then calls Signal(), which returns a fence value.
    // When the GPU reaches that point in the queue, it writes the value into the fence.
    // IsFenceComplete()/WaitForFenceValue() compare against it, so the CPU knows when memory
    // used by that work (command allocators, upload buffers, resources) can be reused or released.
    //
    // Not thread-safe. Not copyable or movable.

    class CommandQueue {
    public:
        CommandQueue(NativeDevice *device, D3D12_COMMAND_LIST_TYPE type, std::wstring_view name);
        // Waits until the GPU has finished all submitted work
        ~CommandQueue();

        CommandQueue(const CommandQueue &) = delete;
        CommandQueue &operator=(const CommandQueue &) = delete;
        CommandQueue(CommandQueue &&) = delete;
        CommandQueue &operator=(CommandQueue &&) = delete;

        // Submits a closed command list. Returns immediately; the GPU runs it later.
        void ExecuteCommandList(ID3D12CommandList *commandList);

        // Adds a "set fence to N" command at the end of the queue and returns N.
        // N is complete once the GPU has finished everything submitted before this call.
        [[nodiscard]] uint64_t Signal();

        [[nodiscard]] bool IsFenceComplete(uint64_t fenceValue) const;

        // Blocks the calling thread until the GPU has reached fenceValue
        void WaitForFenceValue(uint64_t fenceValue);

        // Signal + wait: the GPU is idle afterwards. Only on resize, shutdown and upload completion,
        // never per frame.
        void Flush();

        [[nodiscard]] uint64_t TimestampFrequency() const;

        [[nodiscard]] ID3D12CommandQueue *Native() const noexcept { return m_queue.Get(); }

    private:
        ComPtr<ID3D12CommandQueue> m_queue;
        ComPtr<ID3D12Fence> m_fence;
        uint64_t m_lastSignaledValue = 0; // the fence starts at 0, so value 0 is always complete
        core::UniqueHandle m_fenceEvent;  // signaled by the fence when a waited-for value is reached
    };
} // namespace rhi

#endif //D3D12_ENGINE_COMMANDQUEUE_H
