#include "rhi/UploadContext.h"

#include <string>
#include <utility>

#include <directx/d3dx12.h>

#include "core/Assert.h"
#include "core/ThrowIfFailed.h"
#include "rhi/CommandQueue.h"

namespace engine::rhi {
    using core::ThrowIfFailed;

    UploadContext::UploadContext(NativeDevice *device, Allocator &allocator, std::wstring_view name)
        : m_allocator(&allocator),
          m_commands(device, D3D12_COMMAND_LIST_TYPE_DIRECT, std::wstring(name) + L" command list") {
        // Its own allocator: it is reset when the batch finishes, not when a frame does
        ThrowIfFailed(
            device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
        ThrowIfFailed(m_commandAllocator->SetName((std::wstring(name) + L" command allocator").c_str()));
    }

    void UploadContext::Begin(CommandQueue &queue) {
        ENGINE_ASSERT_MSG(!m_recording, "UploadContext::Begin called while a batch is recording");

        // The previous batch's commands live in the allocator until the GPU has run them
        queue.WaitForFenceValue(m_submittedFenceValue);
        m_submittedStaging.clear();
        ThrowIfFailed(m_commandAllocator->Reset());

        m_commands.Begin(m_commandAllocator.Get());
        m_recording = true;
    }

    UploadAllocation UploadContext::AllocateStaging(uint64_t sizeBytes, std::wstring_view name) {
        ENGINE_ASSERT_MSG(m_recording, "UploadContext::AllocateStaging outside Begin/Submit");
        ENGINE_ASSERT(sizeBytes > 0);

        // Upload heap: the CPU writes it, the GPU reads it. Resources there always stay in GENERIC_READ.
        Allocation staging =
            m_allocator->CreateResource(D3D12_HEAP_TYPE_UPLOAD, CD3DX12_RESOURCE_DESC::Buffer(sizeBytes),
                                        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, name);

        // Stays mapped until released (allowed for upload memory)
        ID3D12Resource *resource = staging->GetResource();
        void *mapped = nullptr;
        const D3D12_RANGE noRead{.Begin = 0, .End = 0}; // the CPU will not read from it
        ThrowIfFailed(resource->Map(0, &noRead, &mapped));

        const UploadAllocation allocation{.cpu = static_cast<std::byte *>(mapped),
                                          .gpu = resource->GetGPUVirtualAddress(),
                                          .resource = resource,
                                          .offset = 0,
                                          .size = sizeBytes};
        m_staging.push_back(std::move(staging));
        m_stagingBytes += sizeBytes;
        return allocation;
    }

    CommandContext &UploadContext::Commands() {
        ENGINE_ASSERT_MSG(m_recording, "UploadContext::Commands outside Begin/Submit");
        return m_commands;
    }

    uint64_t UploadContext::Submit(CommandQueue &queue) {
        ENGINE_ASSERT_MSG(m_recording, "UploadContext::Submit without Begin");
        ENGINE_ASSERT(m_submittedStaging.empty()); // Begin() already freed the previous batch

        m_commands.Close();
        queue.ExecuteCommandList(m_commands.Native());
        m_submittedFenceValue = queue.Signal();
        m_recording = false;

        // The GPU reads the staging buffers later, when it runs the copies: keep them until the fence
        m_submittedStaging = std::move(m_staging);
        m_staging.clear(); // a moved-from vector is not guaranteed to be empty
        m_stagingBytes = 0;
        return m_submittedFenceValue;
    }

    void UploadContext::ReleaseCompleted(const CommandQueue &queue) {
        if (!m_submittedStaging.empty() && queue.IsFenceComplete(m_submittedFenceValue)) {
            m_submittedStaging.clear();
        }
    }
} // namespace engine::rhi
