#include "rhi/DescriptorAllocator.h"

#include <format>
#include <stdexcept>
#include <string>
#include <utility>

#include "core/Assert.h"
#include "core/ThrowIfFailed.h"

namespace rhi {
    using core::ThrowIfFailed;

    Descriptor::~Descriptor() {
        Reset();
    }

    Descriptor::Descriptor(Descriptor &&other) noexcept
        : m_allocator(std::exchange(other.m_allocator, nullptr)), m_index(other.m_index), m_cpu(other.m_cpu) {}

    Descriptor &Descriptor::operator=(Descriptor &&other) noexcept {
        if (this != &other) {
            Reset();
            m_allocator = std::exchange(other.m_allocator, nullptr);
            m_index = other.m_index;
            m_cpu = other.m_cpu;
        }
        return *this;
    }

    void Descriptor::Reset() noexcept {
        if (m_allocator != nullptr) {
            m_allocator->Free(m_index);
            m_allocator = nullptr;
            m_cpu = {};
        }
    }

    DescriptorAllocator::DescriptorAllocator(NativeDevice *device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity,
                                             std::wstring_view name)
        : m_capacity(capacity), m_type(type) {
        const D3D12_DESCRIPTOR_HEAP_DESC desc{
            .Type = type,
            .NumDescriptors = capacity,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0,
        };
        ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)));
        ThrowIfFailed(m_heap->SetName(std::wstring(name).c_str()));

        m_heapStart = m_heap->GetCPUDescriptorHandleForHeapStart();
        m_descriptorSize = device->GetDescriptorHandleIncrementSize(type);

        m_freeIndices.reserve(capacity);
        for (uint32_t i = capacity; i > 0; --i) {
            m_freeIndices.push_back(i - 1);
        }
    }

    DescriptorAllocator::~DescriptorAllocator() {
        ENGINE_ASSERT_MSG(m_freeIndices.size() == m_capacity, "{} descriptor(s) still allocated",
                          m_capacity - m_freeIndices.size());
    }

    Descriptor DescriptorAllocator::Allocate() {
        if (m_freeIndices.empty()) {
            throw std::runtime_error(std::format("Descriptor heap of type {} is full ({} descriptors)",
                                                 static_cast<int>(m_type), m_capacity));
        }
        const uint32_t index = m_freeIndices.back();
        m_freeIndices.pop_back();

        const D3D12_CPU_DESCRIPTOR_HANDLE cpu{m_heapStart.ptr + static_cast<SIZE_T>(index) * m_descriptorSize};
        return Descriptor(this, index, cpu);
    }

    void DescriptorAllocator::Free(uint32_t index) noexcept {
        m_freeIndices.push_back(index);
    }
} // namespace rhi
