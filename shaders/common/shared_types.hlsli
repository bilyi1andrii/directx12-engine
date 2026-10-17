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
#include <DirectXMath.h>

namespace engine::gpu {
    using float3 = DirectX::XMFLOAT3;
    using float4x4 = DirectX::XMFLOAT4X4;
#endif

// Per-frame constants, root CBV b1 (gpu::kRootFrameConstants, common/root_signature.hlsli)
struct FrameConstants
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float3   cameraPosition; // world space
    float    pad0;
};

#ifdef __cplusplus
    static_assert(sizeof(FrameConstants) % 16 == 0, "constant buffer structs must be a multiple of 16 bytes");
} // namespace engine::gpu
#endif

#endif // SHARED_TYPES_HLSLI
