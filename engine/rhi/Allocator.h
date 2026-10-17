#ifndef D3D12_ENGINE_ALLOCATOR_H
#define D3D12_ENGINE_ALLOCATOR_H

#include <string_view>
#include <D3D12MemAlloc.h>
#include "rhi/RhiCommon.h"

namespace engine::rhi {
    class Device;

    // A resource together with the memory it lives in. Releasing the last reference releases both.
    // GetResource() returns the ID3D12Resource without adding a reference.
    using Allocation = ComPtr<D3D12MA::Allocation>;

    // Creates every buffer and texture of the engine.
    //
    // CreateCommittedResource asks the driver for a separate heap per resource, which is slow and wastes
    // memory once there are thousands of resources. D3D12MA allocates large heaps (64 MB by default) and
    // places many resources into each one (CreatePlacedResource); it falls back to committed resources
    // where the driver prefers them.
    //
    // Not copyable or movable.
    class Allocator {
    public:
        explicit Allocator(const Device &device);

        Allocator(const Allocator &) = delete;
        Allocator &operator=(const Allocator &) = delete;
        Allocator(Allocator &&) = delete;
        Allocator &operator=(Allocator &&) = delete;

        // heapType: DEFAULT (GPU-local), UPLOAD (CPU writes, GPU reads) or READBACK(CPU reads).
        // clearValue: the optimized clear value of a render target or depth texture, nullptr otherwise.
        [[nodiscard]] Allocation CreateResource(D3D12_HEAP_TYPE heapType, const D3D12_RESOURCE_DESC &desc,
                                                D3D12_RESOURCE_STATES initialState, const D3D12_CLEAR_VALUE *clearValue,
                                                std::wstring_view name);

    private:
        ComPtr<D3D12MA::Allocator> m_allocator;
    };
} // namespace engine::rhi

#endif // D3D12_ENGINE_ALLOCATOR_H
