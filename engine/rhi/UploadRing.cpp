#include "rhi/UploadRing.h"

#include <bit>
#include <format>
#include <stdexcept>
#include <utility>

#include "core/Assert.h"
#include "core/ThrowIfFailed.h"

namespace engine::rhi {
    using core::ThrowIfFailed;

    namespace {
        // Rounds `value` up to a multiple of `alignment` (a power of two)
        constexpr uint64_t AlignUp(uint64_t value, uint64_t alignment) noexcept {
            return (value + alignment - 1) & ~(alignment - 1);
        }
    } // namespace

    UploadRing::UploadRing(Allocator &allocator, std::wstring_view name, uint64_t pageSize)
        : m_allocator(&allocator), m_name(name), m_pageSize(pageSize) {
        ENGINE_ASSERT_MSG(pageSize % kConstantBufferAlignment == 0, "Upload page size {} is not a multiple of 256",
                          pageSize);
    }

    UploadAllocation UploadRing::Allocate(uint64_t sizeBytes, uint64_t alignment) {
        ENGINE_ASSERT_MSG(std::has_single_bit(alignment), "Upload alignment {} is not a power of two", alignment);
        if (sizeBytes > m_pageSize) {
            throw std::length_error(
                std::format("Upload allocation of {} bytes does not fit in a {}-byte page", sizeBytes, m_pageSize));
        }

        if (m_currentPage == m_pages.size() || !m_pages[m_currentPage].HasSpace(sizeBytes, alignment, m_pageSize)) {
            RequestPage();
        }

        Page &page = m_pages[m_currentPage];
        page.offset = AlignUp(page.offset, alignment);
        const UploadAllocation allocation{.cpu = page.cpuBase + page.offset, .gpu = page.gpuBase + page.offset};
        page.offset += AlignUp(sizeBytes, alignment);
        return allocation;
    }

    void UploadRing::Reset() noexcept {
        for (Page &page : m_pages) {
            page.offset = 0;
        }
        m_currentPage = m_pages.size(); // the next Allocate() starts again at page 0
    }

    void UploadRing::RequestPage() {
        // After Reset() the index is m_pages.size(): wrap to the first page. Otherwise move to the next.
        m_currentPage = (m_currentPage == m_pages.size()) ? 0 : m_currentPage + 1;
        if (m_currentPage < m_pages.size()) {
            return;
        }

        // Every page is in use: create one more
        const D3D12_RESOURCE_DESC desc{
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = 0,
            .Width = m_pageSize,
            .Height = 1,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc = {.Count = 1, .Quality = 0},
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = D3D12_RESOURCE_FLAG_NONE,
        };
        Page page;
        page.allocation = m_allocator->CreateResource(D3D12_HEAP_TYPE_UPLOAD, desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                      nullptr, std::format(L"{} page {}", m_name, m_pages.size()));

        // Upload-heap resources may stay mapped for their whole life,
        // and releasing a mapped resource is allowed. The memory is write-combined: fast to
        // write in order, very slow to read, so the CPU must never read through this pointer.
        ID3D12Resource *resource = page.allocation->GetResource();
        void *mapped = nullptr;
        const D3D12_RANGE noRead{.Begin = 0, .End = 0};
        ThrowIfFailed(resource->Map(0, &noRead, &mapped));
        page.cpuBase = static_cast<std::byte *>(mapped);
        page.gpuBase = resource->GetGPUVirtualAddress();

        m_pages.push_back(std::move(page)); // now m_pages[m_currentPage]
    }

    bool UploadRing::Page::HasSpace(uint64_t sizeBytes, uint64_t alignment, uint64_t pageSize) const noexcept {
        return AlignUp(offset, alignment) + AlignUp(sizeBytes, alignment) <= pageSize;
    }
} // namespace engine::rhi
