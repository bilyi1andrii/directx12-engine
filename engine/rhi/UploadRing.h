#ifndef D3D12_ENGINE_UPLOADRING_H
#define D3D12_ENGINE_UPLOADRING_H

// Per-frame memory the CPU writes and the GPU reads, for data that changes every frame, such as constants and
// instances. Its pages are reused once the GPU has finished the frame.

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

namespace rhi {
    inline constexpr uint64_t kConstantBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;

    struct UploadAllocation {
        std::byte *cpu = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
        ID3D12Resource *resource = nullptr;
        uint64_t offset = 0; // in `resource`
        uint64_t size = 0;   // bytes

        template <GpuCopyable T> void Write(uint64_t byteOffset, const T &value) const {
            WriteBytes(byteOffset, std::as_bytes(std::span(&value, 1)));
        }

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

    class UploadRing {
    public:
        static constexpr uint64_t kPageSize = 2 * 1024 * 1024;

        UploadRing(Allocator &allocator, std::wstring_view name);

        UploadRing(const UploadRing &) = delete;
        UploadRing &operator=(const UploadRing &) = delete;
        UploadRing(UploadRing &&) noexcept = default;
        UploadRing &operator=(UploadRing &&) noexcept = default;

        [[nodiscard]] UploadAllocation Allocate(uint64_t sizeBytes, uint64_t alignment);

        template <GpuCopyable T> [[nodiscard]] UploadAllocation Write(const T &value, uint64_t alignment) {
            const UploadAllocation allocation = Allocate(sizeof(T), alignment);
            allocation.Write(0, value);
            return allocation;
        }

        void Reset() noexcept;

        [[nodiscard]] uint64_t PageSize() const noexcept { return kPageSize; }

    private:
        struct Page {
            Allocation allocation;
            std::byte *cpuBase = nullptr;
            D3D12_GPU_VIRTUAL_ADDRESS gpuBase = 0;
            uint64_t offset = 0;

            [[nodiscard]] bool HasSpace(uint64_t sizeBytes, uint64_t alignment) const noexcept;
        };

        void RequestPage();

        Allocator *m_allocator;
        std::wstring m_name;
        std::vector<Page> m_pages;
        size_t m_currentPage = 0;
    };
} // namespace rhi

#endif // D3D12_ENGINE_UPLOADRING_H
