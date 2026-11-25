// Compiled twice: normal mesh pass (instance world matrix) and SHADOW_CASTER pass (light matrix in b2).
#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"
#include "common/vs_output.hlsli"

ConstantBuffer<DrawConstants> g_draw : register(b0);
ConstantBuffer<FrameConstants> g_frame : register(b1);
#ifdef SHADOW_CASTER
ConstantBuffer<ShadowConstants> g_shadow : register(b2);
#endif

// Must match kVertexLayout in engine/render/passes/VertexLayout.h
struct VSInput
{
    float3 position : POSITION;
    float2 uv       : TEXCOORD;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
};

[RootSignature(GLOBAL_ROOT_SIGNATURE)]
VSOutput main(VSInput input, uint instanceId : SV_InstanceID)
{
    StructuredBuffer<InstanceData> instances = ResourceDescriptorHeap[g_draw.instanceBufferIndex];
    const float4x4 world = instances[g_draw.instanceOffset + instanceId].world;

    VSOutput output;
    const float4 worldPosition = mul(float4(input.position, 1.0f), world);
#ifdef SHADOW_CASTER
    output.position = mul(worldPosition, g_shadow.lightViewProjection);
#else
    output.position = mul(worldPosition, g_frame.viewProjection);
#endif
    output.uv = input.uv;
    const float3 axisX = world[0].xyz;
    const float3 axisY = world[1].xyz;
    const float3 axisZ = world[2].xyz;
    output.normal = normalize(input.normal.x * cross(axisY, axisZ) + input.normal.y * cross(axisZ, axisX) +
                              input.normal.z * cross(axisX, axisY));
    output.tangent = normalize(mul(float4(input.tangent, 0.0f), world).xyz);
    output.worldPosition = worldPosition.xyz;
    return output;
}
