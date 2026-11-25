#include "rhi/Allocator.h"

#include <string>

#include "core/ThrowIfFailed.h"
#include "rhi/Device.h"

namespace rhi {
    using core::ThrowIfFailed;

    Allocator::Allocator(const Device &device) {
        const D3D12MA::ALLOCATOR_DESC desc{
            .Flags = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS,
            .pDevice = device.Native(),
            .PreferredBlockSize = 0,
            .pAllocationCallbacks = nullptr,
            .pAdapter = device.Adapter(),
        };
        ThrowIfFailed(D3D12MA::CreateAllocator(&desc, &m_allocator));
    }

    Allocation Allocator::CreateResource(D3D12_HEAP_TYPE heapType, const D3D12_RESOURCE_DESC &desc,
                                         D3D12_RESOURCE_STATES initialState, const D3D12_CLEAR_VALUE *clearValue,
                                         std::wstring_view name) {
        const D3D12MA::ALLOCATION_DESC allocationDesc{.HeapType = heapType};

        Allocation allocation;
        ThrowIfFailed(m_allocator->CreateResource(&allocationDesc, &desc, initialState, clearValue, &allocation,
                                                  IID_NULL, nullptr));

        const std::wstring nameString(name);
        ThrowIfFailed(allocation->GetResource()->SetName(nameString.c_str()));
        allocation->SetName(nameString.c_str());
        return allocation;
    }
} // namespace rhi
