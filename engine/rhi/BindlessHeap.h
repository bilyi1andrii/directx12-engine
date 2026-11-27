#ifndef D3D12_ENGINE_BINDLESSHEAP_H
#define D3D12_ENGINE_BINDLESSHEAP_H

// The one heap of views that shaders index directly (bindless), with a slot for every texture and buffer they
// read. A slot is freed only after the GPU has finished every frame that may read it.

#include <cstdint>
#include <string_view>
#include <vector>

#include "rhi/RhiCommon.h"

namespace rhi {
    class BindlessHeap {
    public:
        static constexpr uint32_t kDefaultCapacity = 100'000;

        BindlessHeap(NativeDevice *device, uint32_t capacity, std::wstring_view name);
        ~BindlessHeap();

        BindlessHeap(const BindlessHeap &) = delete;
        BindlessHeap &operator=(const BindlessHeap &) = delete;
        BindlessHeap(BindlessHeap &&) = delete;
        BindlessHeap &operator=(BindlessHeap &&) = delete;

        // write view right away, free through FrameContext::DeferFree
        [[nodiscard]] uint32_t Allocate();

        // only after frame finished reading slot
        void Free(uint32_t index) noexcept;

        void CreateStructuredBufferSrv(uint32_t index, ID3D12Resource *buffer, uint64_t firstElement,
                                       uint32_t numElements, uint32_t strideBytes);

        void CreateTexture2DSrv(uint32_t index, ID3D12Resource *texture, DXGI_FORMAT format, uint32_t mipLevels);

        // where view is written
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle(uint32_t index) const noexcept;

        // for descriptor tables, only ImGui uses them
        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle(uint32_t index) const noexcept;

        // ImGui frees by CPU handle
        [[nodiscard]] uint32_t IndexOf(D3D12_CPU_DESCRIPTOR_HANDLE handle) const noexcept;

        [[nodiscard]] ID3D12DescriptorHeap *Native() const noexcept { return m_heap.Get(); }

    private:
        NativeDevice *m_device;
        ComPtr<ID3D12DescriptorHeap> m_heap;
        D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
        D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart{};
        uint32_t m_descriptorSize = 0; // bytes
        uint32_t m_capacity = 0;
        std::vector<uint32_t> m_freeIndices;

        // for double-free asserts
        std::vector<bool> m_allocated;
    };
} // namespace rhi

#endif // D3D12_ENGINE_BINDLESSHEAP_H
