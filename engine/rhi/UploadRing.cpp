#include "rhi/UploadRing.h"

#include <format>
#include <stdexcept>
#include <utility>

#include <directx/d3dx12.h>

#include "core/ThrowIfFailed.h"

namespace rhi {
    using core::ThrowIfFailed;

    namespace {
        constexpr uint64_t AlignUp(uint64_t value, uint64_t alignment) noexcept {
            return (value + alignment - 1) & ~(alignment - 1);
        }
    } // namespace

    UploadBuffer CreateUploadBuffer(Allocator &allocator, uint64_t sizeBytes, std::wstring_view name) {
        Allocation allocation =
            allocator.CreateResource(D3D12_HEAP_TYPE_UPLOAD, CD3DX12_RESOURCE_DESC::Buffer(sizeBytes),
                                     D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, name);

        ID3D12Resource *resource = allocation->GetResource();
        void *mapped = nullptr;
        const D3D12_RANGE noRead{.Begin = 0, .End = 0};
        ThrowIfFailed(resource->Map(0, &noRead, &mapped));

        return {.allocation = std::move(allocation),
                .whole = {.cpu = static_cast<std::byte *>(mapped),
                          .gpu = resource->GetGPUVirtualAddress(),
                          .resource = resource,
                          .offset = 0,
                          .size = sizeBytes}};
    }

    UploadRing::UploadRing(Allocator &allocator, std::wstring_view name) : m_allocator(&allocator), m_name(name) {}

    UploadAllocation UploadRing::Allocate(uint64_t sizeBytes, uint64_t alignment) {
        if (sizeBytes > kPageSize) {
            throw std::length_error(
                std::format("Upload allocation of {} bytes does not fit in a {}-byte page", sizeBytes, kPageSize));
        }

        if (m_currentPage == m_pages.size() || !m_pages[m_currentPage].HasSpace(sizeBytes, alignment)) {
            RequestPage();
        }

        Page &page = m_pages[m_currentPage];
        page.offset = AlignUp(page.offset, alignment);
        const UploadAllocation allocation{.cpu = page.cpuBase + page.offset,
                                          .gpu = page.gpuBase + page.offset,
                                          .resource = page.allocation->GetResource(),
                                          .offset = page.offset,
                                          .size = sizeBytes};
        page.offset += AlignUp(sizeBytes, alignment);
        return allocation;
    }

    void UploadRing::Reset() noexcept {
        for (Page &page : m_pages) {
            page.offset = 0;
        }
        // no page in use yet
        m_currentPage = m_pages.size();
    }

    void UploadRing::RequestPage() {
        m_currentPage = (m_currentPage == m_pages.size()) ? 0 : m_currentPage + 1;
        if (m_currentPage < m_pages.size()) {
            return;
        }

        UploadBuffer buffer =
            CreateUploadBuffer(*m_allocator, kPageSize, std::format(L"{} page {}", m_name, m_pages.size()));
        m_pages.push_back(
            {.allocation = std::move(buffer.allocation), .cpuBase = buffer.whole.cpu, .gpuBase = buffer.whole.gpu});
    }

    bool UploadRing::Page::HasSpace(uint64_t sizeBytes, uint64_t alignment) const noexcept {
        return AlignUp(offset, alignment) + AlignUp(sizeBytes, alignment) <= kPageSize;
    }
} // namespace rhi
