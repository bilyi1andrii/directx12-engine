#ifndef D3D12_ENGINE_UPLOADCONTEXT_H
#define D3D12_ENGINE_UPLOADCONTEXT_H

// Uploads meshes and textures in batches, recorded into one command list. Each upload gets its own staging memory,
// freed once the GPU has run the batch.

#include <cstdint>
#include <string_view>
#include <vector>

#include "rhi/Allocator.h"
#include "rhi/CommandContext.h"
#include "rhi/RhiCommon.h"
#include "rhi/UploadRing.h"

namespace rhi {
    class CommandQueue;

    class UploadContext {
    public:
        UploadContext(NativeDevice *device, Allocator &allocator, std::wstring_view name);

        UploadContext(const UploadContext &) = delete;
        UploadContext &operator=(const UploadContext &) = delete;
        UploadContext(UploadContext &&) = delete;
        UploadContext &operator=(UploadContext &&) = delete;

        void Begin(CommandQueue &queue);

        [[nodiscard]] UploadAllocation AllocateStaging(uint64_t sizeBytes, std::wstring_view name);

        [[nodiscard]] CommandContext &Commands() noexcept { return m_commands; }

        void Submit(CommandQueue &queue);

        void ReleaseCompleted(const CommandQueue &queue);

        [[nodiscard]] bool IsRecording() const noexcept { return m_recording; }

        [[nodiscard]] uint64_t StagingBytes() const noexcept { return m_stagingBytes; }

    private:
        Allocator *m_allocator;
        ComPtr<ID3D12CommandAllocator> m_commandAllocator;
        CommandContext m_commands;
        bool m_recording = false;

        // keeps staging buffers alive until GPU copied them
        std::vector<Allocation> m_staging;
        uint64_t m_stagingBytes = 0;
        std::vector<Allocation> m_submittedStaging;
        uint64_t m_submittedFenceValue = 0;
    };
} // namespace rhi

#endif // D3D12_ENGINE_UPLOADCONTEXT_H
