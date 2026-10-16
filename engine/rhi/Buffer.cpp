#include "rhi/Buffer.h"

#include <cstring>
#include <string>
#include <utility>

#include "core/Assert.h"
#include "core/ThrowIfFailed.h"
#include "rhi/CommandContext.h"
#include "rhi/FrameContext.h"

namespace engine::rhi {
    using core::ThrowIfFailed;

    namespace {
        ComPtr<ID3D12Resource> CreateBufferResource(NativeDevice *device, D3D12_HEAP_TYPE heapType, uint64_t size,
                                                    D3D12_RESOURCE_STATES initialState, const std::wstring &name) {
            const D3D12_HEAP_PROPERTIES heapProperties{.Type = heapType};
            const D3D12_RESOURCE_DESC desc{
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

            // Committed resource until D3D12MA
            ComPtr<ID3D12Resource> resource;
            ThrowIfFailed(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, initialState,
                                                          nullptr, IID_PPV_ARGS(&resource)));
            ThrowIfFailed(resource->SetName(name.c_str()));
            return resource;
        }
    } // namespace

    Buffer Buffer::CreateWithData(NativeDevice *device, CommandContext &context, FrameContext &frame,
                                  std::span<const std::byte> data, D3D12_RESOURCE_STATES finalState,
                                  std::wstring_view name) {
        ENGINE_ASSERT_MSG(!data.empty(), "Buffer::CreateWithData with no data");

        Buffer buffer;
        buffer.m_size = data.size_bytes();

        // Default heap: fast for the GPU, but the CPU cannot write it. Buffers are always created
        // in COMMON (the debug layer ignores any other initial state and warns)
        const std::wstring bufferName(name);
        buffer.m_resource = CreateBufferResource(device, D3D12_HEAP_TYPE_DEFAULT, buffer.m_size,
                                                 D3D12_RESOURCE_STATE_COMMON, bufferName);

        // Upload heap: CPU-writable memory the GPU reads over PCIe. It must stay in GENERIC_READ.
        // The data goes there first, then the GPU copies it into the default-heap buffer.
        ComPtr<ID3D12Resource> upload = CreateBufferResource(
            device, D3D12_HEAP_TYPE_UPLOAD, buffer.m_size, D3D12_RESOURCE_STATE_GENERIC_READ, bufferName + L" upload");
        void *mapped = nullptr;
        const D3D12_RANGE noRead{.Begin = 0, .End = 0}; // the CPU will not read from it
        ThrowIfFailed(upload->Map(0, &noRead, &mapped));
        std::memcpy(mapped, data.data(), data.size_bytes());
        upload->Unmap(0, nullptr); // nullptr: the whole buffer may have been written

        // The copy promotes the buffer from COMMON to COPY_DEST implicitly (no barrier needed).
        // The explicit transition afterwards is optional: buffers decay back to COMMON when the
        // command list has finished and are promoted again on their next use.
        context.CopyBuffer(buffer.m_resource.Get(), 0, upload.Get(), 0, buffer.m_size);
        context.Transition(buffer.m_resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, finalState);

        // The GPU reads the upload buffer when it runs the copy, after this function has returned
        frame.DeferRelease(std::move(upload));
        return buffer;
    }

    D3D12_VERTEX_BUFFER_VIEW Buffer::VertexBufferView(uint32_t strideBytes) const {
        ENGINE_ASSERT(IsValid());
        return {
            .BufferLocation = m_resource->GetGPUVirtualAddress(),
            .SizeInBytes = static_cast<UINT>(m_size),
            .StrideInBytes = strideBytes,
        };
    }

    D3D12_INDEX_BUFFER_VIEW Buffer::IndexBufferView(DXGI_FORMAT format) const {
        ENGINE_ASSERT(IsValid());
        ENGINE_ASSERT(format == DXGI_FORMAT_R16_UINT || format == DXGI_FORMAT_R32_UINT);
        return {
            .BufferLocation = m_resource->GetGPUVirtualAddress(),
            .SizeInBytes = static_cast<UINT>(m_size),
            .Format = format,
        };
    }
} // namespace engine::rhi