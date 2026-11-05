#ifndef D3D12_ENGINE_UPLOADRING_H
#define D3D12_ENGINE_UPLOADRING_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/Assert.h"
#include "rhi/Allocator.h"
#include "rhi/GpuCopyable.h"
#include "rhi/RhiCommon.h"

namespace engine::rhi {
    // Constant buffer views (root CBVs included) must start at a multiple of 256 bytes
    inline constexpr uint64_t kConstantBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;

    // A piece of upload memory: the CPU writes through `cpu`, shaders read from `gpu`.
    // `resource` and `offset` locate it inside its buffer (an UploadRing page or an UploadContext staging buffer),
    // for views (e.g. a bindless structured-buffer SRV, whose first element is offset / stride) and copies.
    //
    // Write data with Write/WriteRange, not through a typed pointer: the mapped memory holds bytes, not C++
    // objects. It is write-combined (fast to write in order, very slow to read), so never read it back.
    // ASan cannot see this memory, so the writes check their bounds themselves (Debug builds).
    struct UploadAllocation {
        std::byte *cpu = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
        ID3D12Resource *resource = nullptr; // owned by the ring or the upload context
        uint64_t offset = 0;                // bytes from the start of `resource`
        uint64_t size = 0;                  // bytes requested

        // Copies `value` to `byteOffset` bytes into this allocation
        template <GpuCopyable T> void Write(uint64_t byteOffset, const T &value) const {
            WriteBytes(byteOffset, std::as_bytes(std::span(&value, 1)));
        }

        // Copies all elements of `values` (an array, vector, span, ...) to `byteOffset` bytes into this allocation
        template <std::ranges::contiguous_range R>
            requires std::ranges::sized_range<R> && GpuCopyable<std::ranges::range_value_t<R>>
        void WriteRange(uint64_t byteOffset, const R &values) const {
            WriteBytes(byteOffset, std::as_bytes(std::span(values)));
        }

    private:
        void WriteBytes(uint64_t byteOffset, std::span<const std::byte> bytes) const {
            ENGINE_ASSERT_MSG(byteOffset <= size && bytes.size() <= size - byteOffset,
                              "Upload write of {} bytes at offset {} overflows a {}-byte allocation", bytes.size(),
                              byteOffset, size);
            std::memcpy(cpu + byteOffset, bytes.data(), bytes.size());
        }
    };

    // Per-frame upload memory pool for frequently updated data (constants, instances).
    // Pages are recycled via Reset() 
    // only after the frame's fence completes. Implements a fixed-size page pool 
    // to eliminate runtime memory allocations after startup.
    // 
    // Move-only. The resource allocator must outlive it.
    class UploadRing {
    public:
        // Page size; also the largest single allocation
        static constexpr uint64_t kDefaultPageSize = 2 * 1024 * 1024;

        UploadRing(Allocator &allocator, std::wstring_view name, uint64_t pageSize = kDefaultPageSize);

        UploadRing(const UploadRing &) = delete;
        UploadRing &operator=(const UploadRing &) = delete;
        UploadRing(UploadRing &&) noexcept = default;
        UploadRing &operator=(UploadRing &&) noexcept = default;

        // Returns `sizeBytes` of upload memory starting at a multiple of `alignment` (a power of two;
        // kConstantBufferAlignment for constant buffers). Valid until the next Reset().
        // Throws std::length_error if `sizeBytes` is larger than a page.
        [[nodiscard]] UploadAllocation Allocate(uint64_t sizeBytes, uint64_t alignment);

        // Allocates sizeof(T) bytes and copies `value` into them, e.g. a constant buffer
        // (alignment kConstantBufferAlignment)
        template <GpuCopyable T> [[nodiscard]] UploadAllocation Write(const T &value, uint64_t alignment) {
            const UploadAllocation allocation = Allocate(sizeof(T), alignment);
            allocation.Write(0, value);
            return allocation;
        }

        // Makes all pages free again. Only after the GPU has finished the frame that used them.
        void Reset() noexcept;

        [[nodiscard]] uint64_t PageSize() const noexcept { return m_pageSize; }

    private:
        // One upload-heap buffer, mapped for its whole life
        struct Page {
            Allocation allocation;
            std::byte *cpuBase = nullptr;
            D3D12_GPU_VIRTUAL_ADDRESS gpuBase = 0;
            uint64_t offset = 0; // first free byte

            [[nodiscard]] bool HasSpace(uint64_t sizeBytes, uint64_t alignment, uint64_t pageSize) const noexcept;
        };

        // Makes the next free page current, creating one if every page is in use
        void RequestPage();

        Allocator *m_allocator;
        std::wstring m_name;
        uint64_t m_pageSize;
        std::vector<Page> m_pages;
        size_t m_currentPage = 0; // index into m_pages; == m_pages.size() when no page is in use yet
    };
} // namespace engine::rhi

#endif // D3D12_ENGINE_UPLOADRING_H
