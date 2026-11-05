#ifndef D3D12_ENGINE_BINDLESSHEAP_H
#define D3D12_ENGINE_BINDLESSHEAP_H

#include <cstdint>
#include <string_view>
#include <vector>

#include "rhi/RhiCommon.h"

namespace engine::rhi {
    // Single global GPU heap for all engine textures/buffers (Bindless / SM 6.6).
    // Resources stay in fixed slots; shaders access them by index via ResourceDescriptorHeap[index].
    //
    // Rules & Threading:
    // - Slots are written once upon Allocate() and must not be overwritten while in use by GPU.
    // - Freed slots must be deferred (FrameContext::DeferFree) until the GPU finishes reading the frame.
    // - Single heap is shared globally because switching heaps on command lists causes GPU stalls.
    // - Stack-based allocation, not thread-safe, non-copyable, non-movable.
    class BindlessHeap {
    public:
        // About 100k descriptors (~3 MB of GPU memory)
        static constexpr uint32_t kDefaultCapacity = 100'000;

        BindlessHeap(NativeDevice *device, uint32_t capacity, std::wstring_view name);
        // Every index must have been freed by now
        ~BindlessHeap();

        BindlessHeap(const BindlessHeap &) = delete;
        BindlessHeap &operator=(const BindlessHeap &) = delete;
        BindlessHeap(BindlessHeap &&) = delete;
        BindlessHeap &operator=(BindlessHeap &&) = delete;

        // A free slot, still empty: write a view into it before any shader reads it. Throws if the heap is full.
        [[nodiscard]] uint32_t Allocate();

        // Returns the slot at once. Only when the GPU can no longer read it; normally through
        // FrameContext::DeferFree, which calls this after the frame's fence.
        void Free(uint32_t index) noexcept;

        // Writes a view of `numElements` elements of `strideBytes` each, starting at element `firstElement`,
        // into slot `index`, for HLSL StructuredBuffer<T> (sizeof(T) == strideBytes).
        void CreateStructuredBufferSrv(uint32_t index, ID3D12Resource *buffer, uint64_t firstElement,
                                       uint32_t numElements, uint32_t strideBytes);

        // Writes a view of all `mipLevels` mips of a 2D texture into slot `index`, for HLSL Texture2D.
        // `format` is the texture's own (an _SRGB format makes samples come back linear).
        void CreateTexture2DSrv(uint32_t index, ID3D12Resource *texture, DXGI_FORMAT format, uint32_t mipLevels);

        [[nodiscard]] ID3D12DescriptorHeap *Native() const noexcept { return m_heap.Get(); }
        [[nodiscard]] uint32_t Capacity() const noexcept { return m_capacity; }
        [[nodiscard]] uint32_t FreeCount() const noexcept { return static_cast<uint32_t>(m_freeIndices.size()); }

    private:
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle(uint32_t index) const noexcept;

        NativeDevice *m_device;
        ComPtr<ID3D12DescriptorHeap> m_heap;
        D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
        uint32_t m_descriptorSize = 0; // bytes between two slots; depends on the GPU
        uint32_t m_capacity = 0;
        std::vector<uint32_t> m_freeIndices; // stack: the next Allocate() takes back()
        std::vector<bool> m_allocated;       // catches double frees and views written into free slots
    };
} // namespace engine::rhi

#endif // D3D12_ENGINE_BINDLESSHEAP_H
