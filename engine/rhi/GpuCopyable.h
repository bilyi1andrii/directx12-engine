#ifndef D3D12_ENGINE_GPUCOPYABLE_H
#define D3D12_ENGINE_GPUCOPYABLE_H

#include <type_traits>

namespace engine::rhi {
    // A type that can be copied byte for byte into memory the GPU reads:
    // - trivially copyable: memcpy is a valid copy;
    // - standard layout: members are laid out in order, so they can match an HLSL struct;
    // - not a pointer: a CPU address means nothing to the GPU.
    // All functions that copy typed data to the GPU require it.
    template <typename T>
    concept GpuCopyable = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T> && !std::is_pointer_v<T>;
} // namespace engine::rhi

#endif // D3D12_ENGINE_GPUCOPYABLE_H
