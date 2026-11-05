#include "rhi/BindlessHeap.h"

#include <format>
#include <stdexcept>
#include <string>

#include "core/Assert.h"
#include "core/ThrowIfFailed.h"

namespace engine::rhi {
    using core::ThrowIfFailed;

    BindlessHeap::BindlessHeap(NativeDevice *device, uint32_t capacity, std::wstring_view name)
        : m_device(device), m_capacity(capacity) {
        ENGINE_ASSERT(capacity > 0);

        // SHADER_VISIBLE: the heap lives in memory the GPU reads while shaders run, so shaders can index it.
        const D3D12_DESCRIPTOR_HEAP_DESC desc{
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = capacity,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            .NodeMask = 0,
        };
        ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)));
        ThrowIfFailed(m_heap->SetName(std::wstring(name).c_str()));

        // Views are written through CPU handles; shaders only need the index
        m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();
        m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // Push in reverse so that the first allocations get slots 0, 1, 2, ... 
        m_freeIndices.reserve(capacity);
        for (uint32_t i = capacity; i > 0; --i) {
            m_freeIndices.push_back(i - 1);
        }
        m_allocated.assign(capacity, false);
    }

    BindlessHeap::~BindlessHeap() {
        ENGINE_ASSERT_MSG(m_freeIndices.size() == m_capacity, "{} bindless index(es) still allocated",
                          m_capacity - m_freeIndices.size());
    }

    uint32_t BindlessHeap::Allocate() {
        if (m_freeIndices.empty()) {
            throw std::runtime_error(std::format("Bindless descriptor heap is full ({} descriptors)", m_capacity));
        }
        const uint32_t index = m_freeIndices.back();
        m_freeIndices.pop_back();
        m_allocated[index] = true;
        return index;
    }

    void BindlessHeap::Free(uint32_t index) noexcept {
        ENGINE_ASSERT_MSG(index < m_capacity && m_allocated[index], "Bindless index {} freed twice or not allocated",
                          index);
        m_allocated[index] = false;
        m_freeIndices.push_back(index);
    }

    void BindlessHeap::CreateStructuredBufferSrv(uint32_t index, ID3D12Resource *buffer, uint64_t firstElement,
                                                 uint32_t numElements, uint32_t strideBytes) {
        ENGINE_ASSERT_MSG(index < m_capacity && m_allocated[index], "Bindless index {} is not allocated", index);

        // A shader resource view (SRV) tells the GPU how to read the buffer: here as an array of
        // `numElements` structs of `strideBytes`, starting at element `firstElement`
        const D3D12_SHADER_RESOURCE_VIEW_DESC desc{
            .Format = DXGI_FORMAT_UNKNOWN, // structured buffers have no format; the stride defines the element
            .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer = {.FirstElement = firstElement,
                       .NumElements = numElements,
                       .StructureByteStride = strideBytes,
                       .Flags = D3D12_BUFFER_SRV_FLAG_NONE},
        };
        m_device->CreateShaderResourceView(buffer, &desc, CpuHandle(index));
    }

    void BindlessHeap::CreateTexture2DSrv(uint32_t index, ID3D12Resource *texture, DXGI_FORMAT format,
                                          uint32_t mipLevels) {
        ENGINE_ASSERT_MSG(index < m_capacity && m_allocated[index], "Bindless index {} is not allocated", index);

        const D3D12_SHADER_RESOURCE_VIEW_DESC desc{
            .Format = format,
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D = {.MostDetailedMip = 0, .MipLevels = mipLevels, .PlaneSlice = 0, .ResourceMinLODClamp = 0.0f},
        };
        m_device->CreateShaderResourceView(texture, &desc, CpuHandle(index));
    }

    D3D12_CPU_DESCRIPTOR_HANDLE BindlessHeap::CpuHandle(uint32_t index) const noexcept {
        // The size of one descriptor differs between GPUs, so slot i is at start + i * size
        return {m_cpuStart.ptr + static_cast<SIZE_T>(index) * m_descriptorSize};
    }
} // namespace engine::rhi
