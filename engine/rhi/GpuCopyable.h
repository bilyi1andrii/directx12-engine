#ifndef D3D12_ENGINE_GPUCOPYABLE_H
#define D3D12_ENGINE_GPUCOPYABLE_H

// The rule for types that may be copied byte for byte into memory the GPU reads: plain data laid out in order,
// and no pointers, because a CPU address means nothing to the GPU.

#include <type_traits>

namespace rhi {
    template <typename T>
    concept GpuCopyable = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T> && !std::is_pointer_v<T>;
} // namespace rhi

#endif // D3D12_ENGINE_GPUCOPYABLE_H
