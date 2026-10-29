// Structs shared between C++ and HLSL ("common/shared_types.hlsli").
//
// Packing Rules (HLSL constant buffers):
//   - Members pack into 16-byte rows and must not straddle a row boundary (float3 needs a float/pad).
//   - Struct sizes must be a multiple of 16 bytes with explicit padding.
//
// Matrices: CPU (DirectXMath, row vectors) transposes before writing; HLSL uses mul(v, M).

#ifndef SHARED_TYPES_HLSLI
#define SHARED_TYPES_HLSLI

#ifdef __cplusplus
#include <bit>
#include <cstdint>

#include <DirectXMath.h>

namespace engine::gpu {
    using uint = uint32_t;
    using float3 = DirectX::XMFLOAT3;
    using float4x4 = DirectX::XMFLOAT4X4;
#endif

// Per-draw root constants b0 (gpu::kRootDrawConstants): at most kDrawConstantCount 32-bit values.
// Root constants live in the command list itself, so they can change between two draws at no memory cost.
struct DrawConstants
{
    uint instanceOffset; // element of the draw's first object in the frame's InstanceData buffer
    uint materialIndex;  // not read yet: there are no materials until textures arrive
};

// Per-frame constants, root CBV b1 (gpu::kRootFrameConstants, common/root_signature.hlsli)
struct FrameConstants
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float3   cameraPosition;      // world space
    uint     instanceBufferIndex; // bindless index of this frame's StructuredBuffer<InstanceData>
};

// Per-object data: one element of the StructuredBuffer<InstanceData> written into the UploadRing each frame.
// A draw reads element instanceOffset (root constant b0) + SV_InstanceID.
struct InstanceData
{
    float4x4 world; // object -> world
};

#ifdef __cplusplus
    static_assert(sizeof(FrameConstants) % 16 == 0, "constant buffer structs must be a multiple of 16 bytes");
    // sizeof(InstanceData) is the SRV stride. The SRV's first element is (upload offset / stride), so instance
    // data is allocated aligned to the stride.
    static_assert(std::has_single_bit(sizeof(InstanceData)), "InstanceData size must be a power of two");
} // namespace engine::gpu
#endif

#endif // SHARED_TYPES_HLSLI
