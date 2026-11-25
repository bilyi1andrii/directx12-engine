#ifndef D3D12_ENGINE_BUFFER_H
#define D3D12_ENGINE_BUFFER_H

// A buffer in GPU memory, such as a mesh's vertices, filled once through an upload batch. It is freed with a
// frame, after the GPU has finished with it.

#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <string_view>

#include "rhi/Allocator.h"
#include "rhi/GpuCopyable.h"
#include "rhi/RhiCommon.h"

namespace rhi {
    class FrameContext;
    class UploadContext;
    struct UploadAllocation;

    class Buffer {
    public:
        Buffer() = default;
        ~Buffer();

        Buffer(const Buffer &) = delete;
        Buffer &operator=(const Buffer &) = delete;
        Buffer(Buffer &&) noexcept = default;
        Buffer &operator=(Buffer &&) = delete;

        [[nodiscard]] static Buffer CreateStaged(Allocator &allocator, UploadContext &upload, uint64_t sizeBytes,
                                                 D3D12_RESOURCE_STATES finalState, std::wstring_view name,
                                                 UploadAllocation &staging);

        template <std::ranges::contiguous_range R>
            requires std::ranges::sized_range<R> && GpuCopyable<std::ranges::range_value_t<R>>
        [[nodiscard]] static Buffer CreateWithData(Allocator &allocator, UploadContext &upload, const R &data,
                                                   D3D12_RESOURCE_STATES finalState, std::wstring_view name) {
            return CreateWithBytes(allocator, upload, std::as_bytes(std::span(data)), finalState, name);
        }

        void Destroy(FrameContext &frame);

        [[nodiscard]] D3D12_VERTEX_BUFFER_VIEW VertexBufferView(uint32_t strideBytes) const;
        [[nodiscard]] D3D12_INDEX_BUFFER_VIEW IndexBufferView(DXGI_FORMAT format) const;

        [[nodiscard]] bool IsValid() const noexcept { return m_allocation != nullptr; }
        [[nodiscard]] ID3D12Resource *Native() const noexcept { return m_allocation->GetResource(); }
        [[nodiscard]] uint64_t Size() const noexcept { return m_size; }

    private:
        [[nodiscard]] static Buffer CreateWithBytes(Allocator &allocator, UploadContext &upload,
                                                    std::span<const std::byte> data, D3D12_RESOURCE_STATES finalState,
                                                    std::wstring_view name);

        Allocation m_allocation;
        uint64_t m_size = 0;
    };
} // namespace rhi

#endif // D3D12_ENGINE_BUFFER_H
