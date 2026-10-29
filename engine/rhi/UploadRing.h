#ifndef D3D12_ENGINE_UPLOADRING_H
#define D3D12_ENGINE_UPLOADRING_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "rhi/Allocator.h"
#include "rhi/RhiCommon.h"

namespace engine::rhi {
    // Constant buffer views (root CBVs included) must start at a multiple of 256 bytes
    inline constexpr uint64_t kConstantBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;

    // A piece of upload memory: the CPU writes through `cpu`, shaders read from `gpu`.
    // `resource` and `offset` locate it inside its page, for views (e.g. a bindless structured-buffer SRV,
    // whose first element is offset / stride).
    struct UploadAllocation {
        std::byte *cpu = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
        ID3D12Resource *resource = nullptr; // the page; owned by the ring
        uint64_t offset = 0;                // bytes from the start of `resource`
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
