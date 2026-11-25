#ifndef D3D12_ENGINE_DESCRIPTORALLOCATOR_H
#define D3D12_ENGINE_DESCRIPTORALLOCATOR_H

// Hands out render-target and depth views from a heap only the CPU sees. Each view gives its slot back when it is
// destroyed.

#include <cstdint>
#include <string_view>
#include <vector>

#include "rhi/RhiCommon.h"

namespace rhi {
    class DescriptorAllocator;

    class Descriptor {
    public:
        Descriptor() = default;
        ~Descriptor();

        Descriptor(const Descriptor &) = delete;
        Descriptor &operator=(const Descriptor &) = delete;
        Descriptor(Descriptor &&other) noexcept;
        Descriptor &operator=(Descriptor &&other) noexcept;

        void Reset() noexcept;

        [[nodiscard]] bool IsValid() const noexcept { return m_allocator != nullptr; }
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE Cpu() const noexcept { return m_cpu; }

    private:
        friend class DescriptorAllocator;
        Descriptor(DescriptorAllocator *allocator, uint32_t index, D3D12_CPU_DESCRIPTOR_HANDLE cpu) noexcept
            : m_allocator(allocator), m_index(index), m_cpu(cpu) {}

        DescriptorAllocator *m_allocator = nullptr;
        uint32_t m_index = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE m_cpu{};
    };

    class DescriptorAllocator {
    public:
        DescriptorAllocator(NativeDevice *device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity,
                            std::wstring_view name);
        ~DescriptorAllocator();

        DescriptorAllocator(const DescriptorAllocator &) = delete;
        DescriptorAllocator &operator=(const DescriptorAllocator &) = delete;
        DescriptorAllocator(DescriptorAllocator &&) = delete;
        DescriptorAllocator &operator=(DescriptorAllocator &&) = delete;

        [[nodiscard]] Descriptor Allocate();

    private:
        friend class Descriptor;
        void Free(uint32_t index) noexcept;

        ComPtr<ID3D12DescriptorHeap> m_heap;
        D3D12_CPU_DESCRIPTOR_HANDLE m_heapStart{};
        uint32_t m_descriptorSize = 0; // bytes
        uint32_t m_capacity = 0;
        D3D12_DESCRIPTOR_HEAP_TYPE m_type{};
        std::vector<uint32_t> m_freeIndices;
    };
} // namespace rhi

#endif // D3D12_ENGINE_DESCRIPTORALLOCATOR_H
