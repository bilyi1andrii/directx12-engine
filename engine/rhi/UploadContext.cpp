#include "rhi/UploadContext.h"

#include <string>
#include <utility>

#include <directx/d3dx12.h>

#include "core/ThrowIfFailed.h"
#include "rhi/CommandQueue.h"

namespace rhi {
    using core::ThrowIfFailed;

    UploadContext::UploadContext(NativeDevice *device, Allocator &allocator, std::wstring_view name)
        : m_allocator(&allocator),
          m_commands(device, D3D12_COMMAND_LIST_TYPE_DIRECT, std::wstring(name) + L" command list") {
        ThrowIfFailed(
            device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
        ThrowIfFailed(m_commandAllocator->SetName((std::wstring(name) + L" command allocator").c_str()));
    }

    void UploadContext::Begin(CommandQueue &queue) {
        queue.WaitForFenceValue(m_submittedFenceValue);
        m_submittedStaging.clear();
        ThrowIfFailed(m_commandAllocator->Reset());

        m_commands.Begin(m_commandAllocator.Get());
        m_recording = true;
    }

    UploadAllocation UploadContext::AllocateStaging(uint64_t sizeBytes, std::wstring_view name) {
        Allocation staging =
            m_allocator->CreateResource(D3D12_HEAP_TYPE_UPLOAD, CD3DX12_RESOURCE_DESC::Buffer(sizeBytes),
                                        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, name);

        ID3D12Resource *resource = staging->GetResource();
        void *mapped = nullptr;
        const D3D12_RANGE noRead{.Begin = 0, .End = 0};
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

    void UploadContext::Submit(CommandQueue &queue) {
        m_commands.Close();
        queue.ExecuteCommandList(m_commands.Native());
        m_submittedFenceValue = queue.Signal();
        m_recording = false;

        m_submittedStaging = std::move(m_staging);
        m_staging.clear();
        m_stagingBytes = 0;
    }

    void UploadContext::ReleaseCompleted(const CommandQueue &queue) {
        if (queue.IsFenceComplete(m_submittedFenceValue)) {
            m_submittedStaging.clear();
        }
    }
} // namespace rhi
