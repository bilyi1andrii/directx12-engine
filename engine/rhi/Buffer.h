#ifndef D3D12_ENGINE_BUFFER_H
#define D3D12_ENGINE_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "rhi/Allocator.h"
#include "rhi/RhiCommon.h"

namespace engine::rhi {
    class CommandContext;
    class FrameContext;

    // A GPU buffer in the default heap (GPU-local memory), e.g. a static vertex or index buffer.
    // Created through rhi::Allocator.
    //
    // Destroying it releases the buffer and its memory at once: the owner must make sure the GPU is idle
    // (Renderer flushes the queue first).
    //
    // Move-only. A default-constructed Buffer is empty.
    class Buffer {
    public:
        Buffer() = default;

        Buffer(const Buffer &) = delete;
        Buffer &operator=(const Buffer &) = delete;
        Buffer(Buffer &&) noexcept = default;
        Buffer &operator=(Buffer &&) noexcept = default;

        // Creates a buffer holding a copy of `data` and records the upload into `context`
        // (which must be recording). The temporary upload buffer goes to `frame`'s deferred-release
        // list, so `frame`'s fence value must be set after the list is submitted. Later commands in
        // this list see the buffer in `finalState`; in later lists it has decayed to COMMON and is
        // promoted again implicitly on use.
        [[nodiscard]] static Buffer CreateWithData(Allocator &allocator, CommandContext &context, FrameContext &frame,
                                                   std::span<const std::byte> data, D3D12_RESOURCE_STATES finalState,
                                                   std::wstring_view name);

        [[nodiscard]] D3D12_VERTEX_BUFFER_VIEW VertexBufferView(uint32_t strideBytes) const;
        [[nodiscard]] D3D12_INDEX_BUFFER_VIEW IndexBufferView(DXGI_FORMAT format) const;

        [[nodiscard]] bool IsValid() const noexcept { return m_allocation != nullptr; }
        [[nodiscard]] ID3D12Resource *Native() const noexcept {
            return m_allocation ? m_allocation->GetResource() : nullptr;
        }
        [[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GpuAddress() const { return Native()->GetGPUVirtualAddress(); }
        [[nodiscard]] uint64_t Size() const noexcept { return m_size; }

    private:
        Allocation m_allocation;
        uint64_t m_size = 0;
    };
} // namespace engine::rhi

#endif // D3D12_ENGINE_BUFFER_H