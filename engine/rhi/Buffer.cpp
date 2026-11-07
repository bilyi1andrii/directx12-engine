#include "rhi/Buffer.h"

#include <exception>
#include <string>
#include <utility>

#include "core/Assert.h"
#include "rhi/CommandContext.h"
#include "rhi/FrameContext.h"
#include "rhi/UploadContext.h"
#include "rhi/UploadRing.h"

namespace engine::rhi {
    namespace {
        D3D12_RESOURCE_DESC BufferDesc(uint64_t size) {
            return {
                .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                .Alignment = 0,
                .Width = size,
                .Height = 1,
                .DepthOrArraySize = 1,
                .MipLevels = 1,
                .Format = DXGI_FORMAT_UNKNOWN, // buffers are untyped; views give the bytes a meaning
                .SampleDesc = {.Count = 1, .Quality = 0},
                .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR, // the only layout buffers can have
                .Flags = D3D12_RESOURCE_FLAG_NONE,
            };
        }
    } // namespace

    Buffer::~Buffer() {
        ENGINE_ASSERT_MSG(!IsValid() || std::uncaught_exceptions() > 0,
                          "Buffer of {} bytes destroyed without Destroy()", m_size);
    }

    void Buffer::Destroy(FrameContext &frame) {
        ENGINE_ASSERT_MSG(IsValid(), "Buffer::Destroy on an empty buffer");
        // Frames still running on the GPU may read it, so it is freed after `frame` finishes
        frame.DeferRelease(std::move(m_allocation));
        m_allocation = nullptr;
        m_size = 0;
    }

    Buffer Buffer::CreateWithBytes(Allocator &allocator, UploadContext &upload, std::span<const std::byte> data,
                                   D3D12_RESOURCE_STATES finalState, std::wstring_view name) {
        ENGINE_ASSERT_MSG(!data.empty(), "Buffer::CreateWithData with no data");

        Buffer buffer;
        buffer.m_size = data.size_bytes();

        // Default heap: fast for the GPU, but the CPU cannot write it. Buffers are always created
        // in COMMON (the debug layer ignores any other initial state and warns)
        const std::wstring bufferName(name);
        buffer.m_allocation = allocator.CreateResource(D3D12_HEAP_TYPE_DEFAULT, BufferDesc(buffer.m_size),
                                                       D3D12_RESOURCE_STATE_COMMON, nullptr, bufferName);

        // The data goes into a staging buffer first; the GPU then copies it into the buffer
        const UploadAllocation staging = upload.AllocateStaging(buffer.m_size, bufferName + L" upload");
        staging.WriteRange(0, data);

        // The copy promotes the buffer from COMMON to COPY_DEST implicitly (no barrier needed).
        // The explicit transition afterwards is optional: buffers decay back to COMMON when the
        // command list has finished and are promoted again on their next use.
        CommandContext &commands = upload.Commands();
        commands.CopyBuffer(buffer.Native(), 0, staging.resource, staging.offset, buffer.m_size);
        commands.Transition(buffer.Native(), D3D12_RESOURCE_STATE_COPY_DEST, finalState);
        return buffer;
    }

    D3D12_VERTEX_BUFFER_VIEW Buffer::VertexBufferView(uint32_t strideBytes) const {
        ENGINE_ASSERT(IsValid());
        return {
            .BufferLocation = GpuAddress(),
            .SizeInBytes = static_cast<UINT>(m_size),
            .StrideInBytes = strideBytes,
        };
    }

    D3D12_INDEX_BUFFER_VIEW Buffer::IndexBufferView(DXGI_FORMAT format) const {
        ENGINE_ASSERT(IsValid());
        ENGINE_ASSERT(format == DXGI_FORMAT_R16_UINT || format == DXGI_FORMAT_R32_UINT);
        return {
            .BufferLocation = GpuAddress(),
            .SizeInBytes = static_cast<UINT>(m_size),
            .Format = format,
        };
    }
} // namespace engine::rhi