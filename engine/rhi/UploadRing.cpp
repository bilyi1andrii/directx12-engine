#include "rhi/UploadRing.h"

#include <format>
#include <stdexcept>
#include <utility>

#include "core/ThrowIfFailed.h"

namespace rhi {
    using core::ThrowIfFailed;

    namespace {
        constexpr uint64_t AlignUp(uint64_t value, uint64_t alignment) noexcept {
            return (value + alignment - 1) & ~(alignment - 1);
        }
    } // namespace

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
        m_currentPage = m_pages.size();
    }

    void UploadRing::RequestPage() {
        m_currentPage = (m_currentPage == m_pages.size()) ? 0 : m_currentPage + 1;
        if (m_currentPage < m_pages.size()) {
            return;
        }

        const D3D12_RESOURCE_DESC desc{
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = 0,
            .Width = kPageSize,
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

        ID3D12Resource *resource = page.allocation->GetResource();
        void *mapped = nullptr;
        const D3D12_RANGE noRead{.Begin = 0, .End = 0};
        ThrowIfFailed(resource->Map(0, &noRead, &mapped));
        page.cpuBase = static_cast<std::byte *>(mapped);
        page.gpuBase = resource->GetGPUVirtualAddress();

        m_pages.push_back(std::move(page));
    }

    bool UploadRing::Page::HasSpace(uint64_t sizeBytes, uint64_t alignment) const noexcept {
        return AlignUp(offset, alignment) + AlignUp(sizeBytes, alignment) <= kPageSize;
    }
} // namespace rhi
