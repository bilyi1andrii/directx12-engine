#ifndef D3D12_ENGINE_UPLOADCONTEXT_H
#define D3D12_ENGINE_UPLOADCONTEXT_H

#include <cstdint>
#include <string_view>
#include <vector>

#include "rhi/Allocator.h"
#include "rhi/CommandContext.h"
#include "rhi/RhiCommon.h"
#include "rhi/UploadRing.h"

namespace engine::rhi {
    class CommandQueue;

    // Records static uploads (mesh buffers, textures) into one command list, a "batch", and submits it.
    //
    // Static data is copied once into GPU-local (default-heap) resources, which the CPU cannot write. Each upload
    // gets its own staging buffer in the upload heap, sized to that upload (not the UploadRing: its largest
    // allocation is one 2 MB page), and the batch's commands copy from there into place. The GPU reads the staging
    // buffers when it runs the batch, after Submit() has returned, so the batch keeps them until its fence value
    // completes.
    //
    // It runs on the direct queue, the queue of the frames: a copy queue cannot transition a texture to
    // PIXEL_SHADER_RESOURCE. On one queue, command lists run in submission order, so an uploaded resource is ready
    // for every list submitted after its batch, with no CPU wait. Rule: submit a batch BEFORE the first frame list
    // that uses its resources (Renderer::EndFrame submits a batch that is still recording). The fence only says when
    // the staging memory can be released and the command allocator reused.
    //
    // Usage: Begin(queue) -> AllocateStaging() and record copies into Commands() -> Submit(queue). Call
    // ReleaseCompleted(queue) once per frame. Always pass the same queue: fence values belong to its fence.
    //
    // The owner waits for the GPU (queue flush) before destroying it; the resource allocator must outlive it.
    // Not copyable or movable.
    class UploadContext {
    public:
        UploadContext(NativeDevice *device, Allocator &allocator, std::wstring_view name);

        UploadContext(const UploadContext &) = delete;
        UploadContext &operator=(const UploadContext &) = delete;
        UploadContext(UploadContext &&) = delete;
        UploadContext &operator=(UploadContext &&) = delete;

        // Starts a batch. If the GPU is still running the previous batch, waits for it first: the command
        // allocator holds that batch's commands. A wait for upload completion, never a per-frame flush.
        void Begin(CommandQueue &queue);

        // `sizeBytes` of mapped upload-heap memory for one upload (`offset` 0, its own buffer), kept until the GPU
        // has finished this batch. Only while recording.
        [[nodiscard]] UploadAllocation AllocateStaging(uint64_t sizeBytes, std::wstring_view name);

        // The batch's command list, for copies and barriers. Only while recording.
        [[nodiscard]] CommandContext &Commands();

        // Closes the batch, executes it on `queue` and returns the fence value that marks its end. Command lists
        // submitted to `queue` afterwards see the uploaded data.
        uint64_t Submit(CommandQueue &queue);

        // Releases the staging memory of the submitted batch if the GPU has finished it. Never waits.
        void ReleaseCompleted(const CommandQueue &queue);

        [[nodiscard]] bool IsRecording() const noexcept { return m_recording; }

    private:
        Allocator *m_allocator;
        ComPtr<ID3D12CommandAllocator> m_commandAllocator;
        CommandContext m_commands;
        bool m_recording = false;

        std::vector<Allocation> m_staging;          // staging buffers of the batch being recorded
        std::vector<Allocation> m_submittedStaging; // staging buffers of the last submitted batch, until its fence
        uint64_t m_submittedFenceValue = 0;         // 0: nothing submitted yet, always complete
    };
} // namespace engine::rhi

#endif // D3D12_ENGINE_UPLOADCONTEXT_H
