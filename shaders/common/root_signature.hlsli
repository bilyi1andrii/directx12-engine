// Global root signature shared by all PSOs. 
// Defined in HLSL to prevent shader/layout drift (compiled to .cso, validated via [RootSignature]).
// C++ includes this file for root parameter indices.
//
// Root Parameters:
//   [0] b0: RootConstants (4x32-bit: drawIndex, materialIndex, spares) - direct per-draw data.
//   [1] b1: Root CBV (FrameConstants) -> UploadRing
//   [2] b2: Root CBV (PassConstants)  -> UploadRing (DATA_STATIC_WHILE_SET_AT_EXECUTE optimization)
//
// Flags: ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT + stage denials.
// TODO(BindlessHeap): Add CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED.
//
// Static Samplers:
//   s0: linear-wrap (textures)
//   s1: point-clamp (post-processing)
//   s2: shadow compare (LESS_EQUAL, white border)

#ifndef ROOT_SIGNATURE_HLSLI
#define ROOT_SIGNATURE_HLSLI

#ifdef __cplusplus
#include <cstdint>

namespace engine::gpu {
    // Root parameter indices for SetGraphicsRoot*: the order of the parameters in GLOBAL_ROOT_SIGNATURE below
    inline constexpr uint32_t kRootDrawConstants = 0;  // b0
    inline constexpr uint32_t kRootFrameConstants = 1; // b1
    inline constexpr uint32_t kRootPassConstants = 2;  // b2

    // Number of 32-bit values in kRootDrawConstants (num32BitConstants below)
    inline constexpr uint32_t kDrawConstantCount = 4;
} // namespace engine::gpu
#else

#define GLOBAL_ROOT_SIGNATURE                                                                   \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT"                                              \
    "        | DENY_HULL_SHADER_ROOT_ACCESS"                                                    \
    "        | DENY_DOMAIN_SHADER_ROOT_ACCESS"                                                  \
    "        | DENY_GEOMETRY_SHADER_ROOT_ACCESS),"                                              \
    "RootConstants(num32BitConstants = 4, b0),"                                                 \
    "CBV(b1, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE),"                                        \
    "CBV(b2, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE),"                                        \
    "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR,"                                     \
    "              addressU = TEXTURE_ADDRESS_WRAP, addressV = TEXTURE_ADDRESS_WRAP,"           \
    "              addressW = TEXTURE_ADDRESS_WRAP, maxAnisotropy = 1,"                         \
    "              comparisonFunc = COMPARISON_NEVER, visibility = SHADER_VISIBILITY_PIXEL),"   \
    "StaticSampler(s1, filter = FILTER_MIN_MAG_MIP_POINT,"                                      \
    "              addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP,"         \
    "              addressW = TEXTURE_ADDRESS_CLAMP, maxAnisotropy = 1,"                        \
    "              comparisonFunc = COMPARISON_NEVER, visibility = SHADER_VISIBILITY_PIXEL),"   \
    "StaticSampler(s2, filter = FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,"                          \
    "              addressU = TEXTURE_ADDRESS_BORDER, addressV = TEXTURE_ADDRESS_BORDER,"       \
    "              addressW = TEXTURE_ADDRESS_BORDER, maxAnisotropy = 1,"                       \
    "              comparisonFunc = COMPARISON_LESS_EQUAL,"                                     \
    "              borderColor = STATIC_BORDER_COLOR_OPAQUE_WHITE,"                             \
    "              visibility = SHADER_VISIBILITY_PIXEL)"

#endif // __cplusplus
#endif // ROOT_SIGNATURE_HLSLI
