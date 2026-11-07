// Mesh vertex shader, ported from DX11 vs_instanced.hlsl.
// Matrix convention: CPU transposes (DirectXMath row-vector), HLSL uses mul(v, M).

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"
#include "common/vs_output.hlsli"

// Per-draw root constants (b0)
ConstantBuffer<DrawConstants> g_draw : register(b0);
// Per-frame constants (b1)
ConstantBuffer<FrameConstants> g_frame : register(b1);

// Must match Vertex and its input layout in Renderer.cpp
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
    // The object's world matrix from the draw's instance buffer
    StructuredBuffer<InstanceData> instances = ResourceDescriptorHeap[g_draw.instanceBufferIndex];
    const float4x4 world = instances[g_draw.instanceOffset + instanceId].world;

    VSOutput output;
    const float4 worldPosition = mul(float4(input.position, 1.0f), world);
    output.position = mul(worldPosition, g_frame.viewProjection);
    output.uv = input.uv;
    // Same as DX11: wrong for non-uniformly scaled objects. Fixed with lighting in step 3.
    output.normal = normalize(mul(float4(input.normal, 0.0f), world).xyz);
    output.tangent = normalize(mul(float4(input.tangent, 0.0f), world).xyz);
    output.worldPosition = worldPosition.xyz;
    return output;
}
