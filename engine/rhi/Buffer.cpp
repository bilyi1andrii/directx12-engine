#include "rhi/Buffer.h"

#include <exception>
#include <string>
#include <utility>

#include <directx/d3dx12.h>

#include "core/Assert.h"
#include "rhi/CommandContext.h"
#include "rhi/FrameContext.h"
#include "rhi/UploadContext.h"
#include "rhi/UploadRing.h"

namespace rhi {
    Buffer::~Buffer() {
        ENGINE_ASSERT_MSG(!IsValid() || std::uncaught_exceptions() > 0,
                          "Buffer of {} bytes destroyed without Destroy()", m_size);
    }

    void Buffer::Destroy(FrameContext &frame) {
        frame.DeferRelease(std::move(m_allocation));
        m_size = 0;
    }

    Buffer Buffer::CreateStaged(Allocator &allocator, UploadContext &upload, uint64_t sizeBytes,
                                D3D12_RESOURCE_STATES finalState, std::wstring_view name, UploadAllocation &staging) {
        Buffer buffer;
        buffer.m_size = sizeBytes;

        const std::wstring bufferName(name);
        buffer.m_allocation =
            allocator.CreateResource(D3D12_HEAP_TYPE_DEFAULT, CD3DX12_RESOURCE_DESC::Buffer(sizeBytes),
                                     D3D12_RESOURCE_STATE_COMMON, nullptr, bufferName);

        staging = upload.AllocateStaging(buffer.m_size, bufferName + L" upload");

        CommandContext &commands = upload.Commands();
        commands.CopyBuffer(buffer.Native(), 0, staging.resource, staging.offset, buffer.m_size);
        commands.Transition(buffer.Native(), D3D12_RESOURCE_STATE_COPY_DEST, finalState);
        return buffer;
    }

    Buffer Buffer::CreateWithBytes(Allocator &allocator, UploadContext &upload, std::span<const std::byte> data,
                                   D3D12_RESOURCE_STATES finalState, std::wstring_view name) {
        UploadAllocation staging;
        Buffer buffer = CreateStaged(allocator, upload, data.size_bytes(), finalState, name, staging);
        staging.WriteRange(0, data);
        return buffer;
    }

    D3D12_VERTEX_BUFFER_VIEW Buffer::VertexBufferView(uint32_t strideBytes) const {
        return {
            .BufferLocation = Native()->GetGPUVirtualAddress(),
            .SizeInBytes = static_cast<UINT>(m_size),
            .StrideInBytes = strideBytes,
        };
    }

    D3D12_INDEX_BUFFER_VIEW Buffer::IndexBufferView(DXGI_FORMAT format) const {
        return {
            .BufferLocation = Native()->GetGPUVirtualAddress(),
            .SizeInBytes = static_cast<UINT>(m_size),
            .Format = format,
        };
    }
} // namespace rhi
