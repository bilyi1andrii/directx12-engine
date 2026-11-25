#ifndef D3D12_ENGINE_DESCRIPTORALLOCATOR_H
#define D3D12_ENGINE_DESCRIPTORALLOCATOR_H

#include <cstdint>
#include <string_view>
#include <vector>

#include "rhi/RhiCommon.h"

namespace rhi {
    class DescriptorAllocator;

    // One slot in a CPU-only descriptor heap. Gives the slot back to its allocator when destroyed.
    // Move-only. Must not outlive the allocator it came from.
    class Descriptor {
    public:
        Descriptor() = default;
        ~Descriptor();

        Descriptor(const Descriptor &) = delete;
        Descriptor &operator=(const Descriptor &) = delete;
        Descriptor(Descriptor &&other) noexcept;
        Descriptor &operator=(Descriptor &&other) noexcept;

        // Returns the slot now. Safe right after recording commands that used it: CPU-only
        // descriptors are copied into the command list when the command is recorded.
        void Reset() noexcept;

        [[nodiscard]] bool IsValid() const noexcept { return m_allocator != nullptr; }
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE Cpu() const noexcept { return m_cpu; }

    private:
        friend class DescriptorAllocator;
        Descriptor(DescriptorAllocator *allocator, uint32_t index, D3D12_CPU_DESCRIPTOR_HANDLE cpu) noexcept
            : m_allocator(allocator), m_index(index), m_cpu(cpu) {}

        DescriptorAllocator *m_allocator = nullptr; // nullptr: empty
        uint32_t m_index = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE m_cpu{};
    };

    // Hands out single descriptors from one fixed-size, CPU-only (not shader-visible) heap.
    // Used for RTVs and DSVs: the GPU never reads these heaps; OMSetRenderTargets, ClearRenderTargetView
    // and friends copy the descriptor when the command is recorded, so a freed slot can be reused at once.
    //
    // The free slots are kept as a stack of indices: Allocate pops, Free pushes, both O(1).
    // Not thread-safe. Not copyable or movable (descriptors point back to it).
    class DescriptorAllocator {
    public:
        DescriptorAllocator(NativeDevice *device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity,
                            std::wstring_view name);
        // Every descriptor must have been returned by now (asserted)
        ~DescriptorAllocator();

        DescriptorAllocator(const DescriptorAllocator &) = delete;
        DescriptorAllocator &operator=(const DescriptorAllocator &) = delete;
        DescriptorAllocator(DescriptorAllocator &&) = delete;
        DescriptorAllocator &operator=(DescriptorAllocator &&) = delete;

        // Throws if the heap is full
        [[nodiscard]] Descriptor Allocate();

        [[nodiscard]] uint32_t Capacity() const noexcept { return m_capacity; }
        [[nodiscard]] uint32_t FreeCount() const noexcept { return static_cast<uint32_t>(m_freeIndices.size()); }

    private:
        friend class Descriptor;
        void Free(uint32_t index) noexcept;

        ComPtr<ID3D12DescriptorHeap> m_heap;
        D3D12_CPU_DESCRIPTOR_HANDLE m_heapStart{};
        uint32_t m_descriptorSize = 0; // bytes between two slots; depends on the GPU and heap type
        uint32_t m_capacity = 0;
        D3D12_DESCRIPTOR_HEAP_TYPE m_type{};
        std::vector<uint32_t> m_freeIndices; // stack: the next Allocate() takes back()
        std::vector<bool> m_allocated;       // catches double frees
    };
} // namespace rhi

#endif // D3D12_ENGINE_DESCRIPTORALLOCATOR_H
