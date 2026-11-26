#ifndef D3D12_ENGINE_ALLOCATOR_H
#define D3D12_ENGINE_ALLOCATOR_H

// Creates every buffer and texture. Many resources share a few large blocks of GPU memory, which is faster than
// one block per resource.

#include <string_view>
#include <D3D12MemAlloc.h>
#include "rhi/RhiCommon.h"

namespace rhi {
    class Device;

    using Allocation = ComPtr<D3D12MA::Allocation>;

    class Allocator {
    public:
        explicit Allocator(const Device &device);

        Allocator(const Allocator &) = delete;
        Allocator &operator=(const Allocator &) = delete;
        Allocator(Allocator &&) = delete;
        Allocator &operator=(Allocator &&) = delete;

        [[nodiscard]] Allocation CreateResource(D3D12_HEAP_TYPE heapType, const D3D12_RESOURCE_DESC &desc,
                                                D3D12_RESOURCE_STATES initialState, const D3D12_CLEAR_VALUE *clearValue,
                                                std::wstring_view name);

    private:
        ComPtr<D3D12MA::Allocator> m_allocator;
    };
} // namespace rhi

#endif // D3D12_ENGINE_ALLOCATOR_H
