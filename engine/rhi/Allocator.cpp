#include "rhi/Allocator.h"

#include <string>

#include "core/ThrowIfFailed.h"
#include "rhi/Device.h"

namespace rhi {
    using core::ThrowIfFailed;

    Allocator::Allocator(const Device &device) {
        // Recommended flags: new heaps are not zeroed by the OS,
        // and MSAA textures get their own heaps so the shared heaps can use the
        // smaller 64 KB alignment
        const D3D12MA::ALLOCATOR_DESC desc{
            .Flags = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS,
            .pDevice = device.Native(),
            .PreferredBlockSize = 0, // default (64 MB)
            .pAllocationCallbacks = nullptr,
            .pAdapter = device.Adapter(), // for the memory budget
        };
        ThrowIfFailed(D3D12MA::CreateAllocator(&desc, &m_allocator));
    }

    Allocation Allocator::CreateResource(D3D12_HEAP_TYPE heapType, const D3D12_RESOURCE_DESC &desc,
                                         D3D12_RESOURCE_STATES initialState, const D3D12_CLEAR_VALUE *clearValue,
                                         std::wstring_view name) {
        const D3D12MA::ALLOCATION_DESC allocationDesc{.HeapType = heapType};

        Allocation allocation;
        // IID_NULL: no extra reference to the resource; the allocation keeps it alive
        ThrowIfFailed(m_allocator->CreateResource(&allocationDesc, &desc, initialState, clearValue, &allocation,
                                                  IID_NULL, nullptr));

        const std::wstring nameString(name);
        ThrowIfFailed(allocation->GetResource()->SetName(nameString.c_str()));
        allocation->SetName(nameString.c_str()); // shows up in D3D12MA's statistics and leak reports
        return allocation;
    }
} // namespace rhi
