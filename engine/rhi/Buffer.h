#ifndef D3D12_ENGINE_BUFFER_H
#define D3D12_ENGINE_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <string_view>

#include "core/Assert.h"
#include "rhi/Allocator.h"
#include "rhi/GpuCopyable.h"
#include "rhi/RhiCommon.h"

namespace engine::rhi {
    class FrameContext;
    class UploadContext;
    struct UploadAllocation;

    // A GPU buffer in the default heap (GPU-local memory), e.g. a static vertex or index buffer.
    // Created through rhi::Allocator.
    //
    // Release it with Destroy(), GPU may still be using it, so the release has to
    // wait for a frame. The destructor checks that Destroy() was called.
    class Buffer {
    public:
        Buffer() = default;
        ~Buffer();

        Buffer(const Buffer &) = delete;
        Buffer &operator=(const Buffer &) = delete;
        Buffer(Buffer &&) noexcept = default;
        Buffer &operator=(Buffer &&) = delete;

        // Creates a buffer of `sizeBytes` and records a copy into it from `staging`, which the caller fills before
        // the upload batch is submitted (the GPU copies only then). Avoids an extra CPU copy of generated data.
        [[nodiscard]] static Buffer CreateStaged(Allocator &allocator, UploadContext &upload, uint64_t sizeBytes,
                                                 D3D12_RESOURCE_STATES finalState, std::wstring_view name,
                                                 UploadAllocation &staging);

        // Creates a buffer holding a copy of `data` (an array, vector or span) and records the copy into the
        // upload batch. The buffer is ready once that batch has been submitted.
        template <std::ranges::contiguous_range R>
            requires std::ranges::sized_range<R> && GpuCopyable<std::ranges::range_value_t<R>>
        [[nodiscard]] static Buffer CreateWithData(Allocator &allocator, UploadContext &upload, const R &data,
                                                   D3D12_RESOURCE_STATES finalState, std::wstring_view name) {
            return CreateWithBytes(allocator, upload, std::as_bytes(std::span(data)), finalState, name);
        }

        // Frees the buffer once the GPU has finished `frame`
        void Destroy(FrameContext &frame);

        [[nodiscard]] D3D12_VERTEX_BUFFER_VIEW VertexBufferView(uint32_t strideBytes) const;
        [[nodiscard]] D3D12_INDEX_BUFFER_VIEW IndexBufferView(DXGI_FORMAT format) const;

        [[nodiscard]] bool IsValid() const noexcept { return m_allocation != nullptr; }
        [[nodiscard]] ID3D12Resource *Native() const noexcept {
            return m_allocation ? m_allocation->GetResource() : nullptr;
        }
        [[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GpuAddress() const {
            ENGINE_ASSERT(IsValid());
            return Native()->GetGPUVirtualAddress();
        }
        [[nodiscard]] uint64_t Size() const noexcept { return m_size; }

    private:
        [[nodiscard]] static Buffer CreateWithBytes(Allocator &allocator, UploadContext &upload,
                                                    std::span<const std::byte> data, D3D12_RESOURCE_STATES finalState,
                                                    std::wstring_view name);

        Allocation m_allocation;
        uint64_t m_size = 0;
    };
} // namespace engine::rhi

#endif // D3D12_ENGINE_BUFFER_H