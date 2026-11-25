#ifndef D3D12_ENGINE_UPLOADCONTEXT_H
#define D3D12_ENGINE_UPLOADCONTEXT_H

#include <cstdint>
#include <string_view>
#include <vector>

#include "rhi/Allocator.h"
#include "rhi/CommandContext.h"
#include "rhi/RhiCommon.h"
#include "rhi/UploadRing.h"

namespace rhi {
    class CommandQueue;

    // Uploads static data (meshes, textures) into GPU memory. The copies are recorded into one command
    // list, a "batch", and each upload gets its own staging buffer that is freed once the GPU has run it.
    //
    // Usage: Begin -> AllocateStaging + record copies into Commands() -> Submit. Call ReleaseCompleted once per frame.
    //
    // Rule: submit the batch before any frame that uses its resources. Both run on the same queue, so the
    // GPU then finishes the copies first and nothing has to wait on the CPU.
    //
    // Destroy it only after the GPU is idle. Always pass the same queue.
    class UploadContext {
    public:
        UploadContext(NativeDevice *device, Allocator &allocator, std::wstring_view name);

        UploadContext(const UploadContext &) = delete;
        UploadContext &operator=(const UploadContext &) = delete;
        UploadContext(UploadContext &&) = delete;
        UploadContext &operator=(UploadContext &&) = delete;

        // Starts a batch; first waits for the previous one if the GPU is still running it
        void Begin(CommandQueue &queue);

        // Mapped upload memory for one upload, kept until the GPU has finished this batch
        [[nodiscard]] UploadAllocation AllocateStaging(uint64_t sizeBytes, std::wstring_view name);

        // The batch's command list, for copies and barriers
        [[nodiscard]] CommandContext &Commands();

        // Executes the batch. Its fence value stays inside: ReleaseCompleted checks it.
        void Submit(CommandQueue &queue);

        // Frees the staging buffers of a finished batch. Never waits.
        void ReleaseCompleted(const CommandQueue &queue);

        [[nodiscard]] bool IsRecording() const noexcept { return m_recording; }

        // Active batch staging memory. Callers check this to submit before the buffer overflows.
        [[nodiscard]] uint64_t StagingBytes() const noexcept { return m_stagingBytes; }

    private:
        Allocator *m_allocator;
        ComPtr<ID3D12CommandAllocator> m_commandAllocator;
        CommandContext m_commands;
        bool m_recording = false;

        std::vector<Allocation> m_staging;          // staging buffers of the batch being recorded
        uint64_t m_stagingBytes = 0;                // their total size
        std::vector<Allocation> m_submittedStaging; // staging buffers of the last submitted batch, until its fence
        uint64_t m_submittedFenceValue = 0;         // 0: nothing submitted yet, always complete
    };
} // namespace rhi

#endif // D3D12_ENGINE_UPLOADCONTEXT_H
