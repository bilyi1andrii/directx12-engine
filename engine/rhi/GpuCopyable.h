#ifndef D3D12_ENGINE_GPUCOPYABLE_H
#define D3D12_ENGINE_GPUCOPYABLE_H

#include <type_traits>

namespace engine::rhi {
    // A type whose bytes can be copied as they are into memory the GPU reads (upload heap, root constants):
    // - trivially copyable: memcpy is a valid copy (no user-written copy, no virtual functions);
    // - standard layout: members lie in declaration order with no hidden data, so their offsets can match an
    //   HLSL struct (the packing itself is checked by the static_asserts in shaders/common/shared_types.hlsli);
    // - not a pointer: a CPU address means nothing to the GPU.
    //
    // The upload APIs (UploadAllocation::Write, UploadRing::Write, Buffer::CreateWithData,
    // CommandContext::SetGraphicsRootConstants) only accept such types, so a typed copy cannot skip the check.
    template <typename T>
    concept GpuCopyable = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T> && !std::is_pointer_v<T>;
} // namespace engine::rhi

#endif // D3D12_ENGINE_GPUCOPYABLE_H
