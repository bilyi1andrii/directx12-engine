#ifndef D3D12_ENGINE_BINDLESSHEAP_H
#define D3D12_ENGINE_BINDLESSHEAP_H

#include <cstdint>
#include <string_view>
#include <vector>

#include "rhi/RhiCommon.h"

namespace engine::rhi {
    // The one shader-visible heap for all textures and buffers (bindless, SM 6.6). There is only one because switching
    // heaps on a command list can stall the GPU. Resources stay in fixed slots, and shaders read them by index through
    // ResourceDescriptorHeap[index].
    //
    // Write a view into a slot right after Allocate(), and never overwrite a slot the GPU may still read. Free slots
    // through FrameContext::DeferFree, so they stay taken until the GPU has finished the frame.
    //
    // Not thread-safe. Not copyable or movable.
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

        // Writes a view of a 2D texture with all its mips into slot `index`, for HLSL Texture2D
        void CreateTexture2DSrv(uint32_t index, ID3D12Resource *texture, DXGI_FORMAT format, uint32_t mipLevels);

        // Handles of slot `index`, for code that binds descriptors itself instead of indexing the heap in HLSL
        // (the ImGui backend: it writes its view through the CPU handle and binds a descriptor table at the GPU
        // handle). Engine code uses the index and the Create*Srv functions.
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle(uint32_t index) const noexcept;
        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle(uint32_t index) const noexcept;
        // The slot a CPU handle of this heap points at (inverse of CpuHandle)
        [[nodiscard]] uint32_t IndexOf(D3D12_CPU_DESCRIPTOR_HANDLE handle) const noexcept;

        [[nodiscard]] ID3D12DescriptorHeap *Native() const noexcept { return m_heap.Get(); }
        [[nodiscard]] uint32_t Capacity() const noexcept { return m_capacity; }
        [[nodiscard]] uint32_t FreeCount() const noexcept { return static_cast<uint32_t>(m_freeIndices.size()); }

    private:
        NativeDevice *m_device;
        ComPtr<ID3D12DescriptorHeap> m_heap;
        D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
        D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart{}; // only shader-visible heaps have GPU handles
        uint32_t m_descriptorSize = 0;            // bytes between two slots; depends on the GPU
        uint32_t m_capacity = 0;
        std::vector<uint32_t> m_freeIndices; // stack: the next Allocate() takes back()
        std::vector<bool> m_allocated;       // catches double frees and views written into free slots
    };
} // namespace engine::rhi

#endif // D3D12_ENGINE_BINDLESSHEAP_H
