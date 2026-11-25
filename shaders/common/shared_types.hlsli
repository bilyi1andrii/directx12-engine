// The data the CPU writes and the shaders read, defined once for both languages. C++ compiles this file too, so
// both sides always agree on the layout.

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

struct DrawConstants
{
    uint instanceOffset;
    uint materialIndex;
    uint instanceBufferIndex;
    uint alphaInfo;
};

static const uint kMaxPointLights = 8;
static const uint kMaxSpotLights = 4;


struct PointLightData
{
    float3 position;
    float  strength;
    float3 color;
    float  pad0;
    float3 attenuation;
    float  pad1;
};

struct SpotLightData
{
    float3 position;
    float  strength;
    float3 direction;
    float  innerCos;
    float3 color;
    float  outerCos;
    float3 attenuation;
    float  pad0;
};

struct FrameConstants
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float3   cameraPosition;
    uint     pointLightCount;

    float3 ambientColor;
    float  ambientStrength;
    float3 specularColor;
    float  shininess;

    float3 directionalDirection;
    float  directionalStrength;
    float3 directionalColor;
    uint   spotLightCount;

    PointLightData pointLights[kMaxPointLights];
    SpotLightData  spotLights[kMaxSpotLights];
};

struct InstanceData
{
    float4x4 world;
};

#ifdef __cplusplus
    static_assert(sizeof(FrameConstants) % 16 == 0, "constant buffer structs must be a multiple of 16 bytes");
    static_assert(sizeof(PointLightData) % 16 == 0, "constant buffer structs must be a multiple of 16 bytes");
    static_assert(sizeof(SpotLightData) % 16 == 0, "constant buffer structs must be a multiple of 16 bytes");
    static_assert(std::has_single_bit(sizeof(InstanceData)), "InstanceData size must be a power of two");
} // namespace engine::gpu
#endif

#endif // SHARED_TYPES_HLSLI