// The root signature every pipeline shares, defined in HLSL so DXC checks each shader against it. C++ includes
// this file for the parameter indices.

#ifndef ROOT_SIGNATURE_HLSLI
#define ROOT_SIGNATURE_HLSLI

#ifdef __cplusplus
#include <cstdint>

namespace gpu {
    inline constexpr uint32_t kRootDrawConstants = 0;  // b0
    inline constexpr uint32_t kRootFrameConstants = 1; // b1
    inline constexpr uint32_t kRootPassConstants = 2;  // b2

    inline constexpr uint32_t kDrawConstantCount = 4; // 32-bit values in b0
}
#else

// DIRECTLY_INDEXED lets shaders read ResourceDescriptorHeap[i]
// DATA_STATIC_WHILE_SET_AT_EXECUTE promises CBV data won't change while GPU uses it
#define GLOBAL_ROOT_SIGNATURE                                                                   \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT"                                              \
    "        | CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED"                                               \
    "        | DENY_HULL_SHADER_ROOT_ACCESS"                                                    \
    "        | DENY_DOMAIN_SHADER_ROOT_ACCESS"                                                  \
    "        | DENY_GEOMETRY_SHADER_ROOT_ACCESS),"                                              \
    "RootConstants(num32BitConstants = 4, b0),"                                                 \
    "CBV(b1, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE),"                                        \
    "CBV(b2, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE),"                                        \
    "StaticSampler(s0, filter = FILTER_ANISOTROPIC,"                                            \
    "              addressU = TEXTURE_ADDRESS_WRAP, addressV = TEXTURE_ADDRESS_WRAP,"           \
    "              addressW = TEXTURE_ADDRESS_WRAP, maxAnisotropy = 16,"                        \
    "              comparisonFunc = COMPARISON_NEVER, visibility = SHADER_VISIBILITY_PIXEL),"   \
    "StaticSampler(s1, filter = FILTER_MIN_MAG_MIP_POINT,"                                      \
    "              addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP,"         \
    "              addressW = TEXTURE_ADDRESS_CLAMP, maxAnisotropy = 1,"                        \
    "              comparisonFunc = COMPARISON_NEVER, visibility = SHADER_VISIBILITY_PIXEL),"   \
    "StaticSampler(s2, filter = FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,"                          \
    "              addressU = TEXTURE_ADDRESS_BORDER, addressV = TEXTURE_ADDRESS_BORDER,"       \
    "              addressW = TEXTURE_ADDRESS_BORDER, maxAnisotropy = 1,"                       \
    "              comparisonFunc = COMPARISON_GREATER_EQUAL,"                                  \
    "              borderColor = STATIC_BORDER_COLOR_OPAQUE_BLACK,"                             \
    "              visibility = SHADER_VISIBILITY_PIXEL)"

#endif // __cplusplus
#endif // ROOT_SIGNATURE_HLSLI
