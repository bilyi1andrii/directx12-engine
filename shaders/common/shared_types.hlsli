// Structs shared between C++ and HLSL.
//
// In HLSL constant buffers, members pack into 16-byte rows and must not cross a row boundary, so a float3 needs a
// float or a padding field after it. Struct sizes must be a multiple of 16 bytes, with explicit padding fields.
//
// The CPU (DirectXMath, row vectors) transposes matrices before writing them; HLSL uses mul(v, M).

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
    uint instanceOffset;      // element of the draw's first object in its InstanceData buffer
    uint materialIndex;       // for now: bindless index of the albedo texture
    uint instanceBufferIndex; // bindless index of the draw's StructuredBuffer<InstanceData>
};

// How many point and spot lights one frame can have. The CPU keeps the first ones and drops the rest.
static const uint kMaxPointLights = 8;
static const uint kMaxSpotLights = 4;

// A light that shines from one point in all directions
struct PointLightData
{
    float3 position; // world space
    float  strength;
    float3 color;
    float  pad0;
    float3 attenuation; // light / (x + y * distance + z * distance^2)
    float  pad1;
};

// A light that shines from one point in a cone: full strength inside the inner cone, fading to nothing at the outer
struct SpotLightData
{
    float3 position; // world space
    float  strength;
    float3 direction; // where it points, world space, unit length
    float  innerCos;  // cosine of the inner cone's half angle; always greater than outerCos
    float3 color;
    float  outerCos;
    float3 attenuation; // as in PointLightData
    float  pad0;
};

// Per-frame constants, root CBV b1 (gpu::kRootFrameConstants, common/root_signature.hlsli).
// The lights are filled by render::PackLights (renderdata/LightPacking.h).
struct FrameConstants
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float3   cameraPosition; // world space
    uint     pointLightCount;

    // Light that reaches every surface from all sides
    float3 ambientColor;
    float  ambientStrength;
    // Highlights: their colour and how tight they are. The same for every material for now.
    float3 specularColor;
    float  shininess;

    // One light without a position, like the sun
    float3 directionalDirection; // where it shines, world space, unit length
    float  directionalStrength;  // 0 when the light is off
    float3 directionalColor;
    uint   spotLightCount;

    PointLightData pointLights[kMaxPointLights]; // the first pointLightCount are used
    SpotLightData  spotLights[kMaxSpotLights];   // the first spotLightCount are used
};

// Per-object data: one element of a StructuredBuffer<InstanceData>. Objects that move are written into the UploadRing
// every frame, and static lists once into a GPU buffer.
// A draw reads element instanceOffset + SV_InstanceID of buffer instanceBufferIndex (both root constants b0).
struct InstanceData
{
    float4x4 world; // object -> world
};

#ifdef __cplusplus
    static_assert(sizeof(FrameConstants) % 16 == 0, "constant buffer structs must be a multiple of 16 bytes");
    // HLSL starts every array element on a new 16-byte row
    static_assert(sizeof(PointLightData) % 16 == 0, "constant buffer structs must be a multiple of 16 bytes");
    static_assert(sizeof(SpotLightData) % 16 == 0, "constant buffer structs must be a multiple of 16 bytes");
    // sizeof(InstanceData) is the SRV stride. The SRV's first element is (upload offset / stride), so instance
    // data is allocated aligned to the stride.
    static_assert(std::has_single_bit(sizeof(InstanceData)), "InstanceData size must be a power of two");
} // namespace engine::gpu
#endif

#endif // SHARED_TYPES_HLSLI
