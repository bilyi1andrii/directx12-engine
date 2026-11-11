// Vertex shader for meshes: reads each object's world matrix from its instance buffer.
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
    // A normal must stay at a right angle to its surface. Multiplying it by `world` does not keep that when an
    // object is scaled more along one axis than another. These three cross products of world's rows do
    // (they are the cofactor matrix of its 3x3 part), without a second matrix per object.
    const float3 axisX = world[0].xyz;
    const float3 axisY = world[1].xyz;
    const float3 axisZ = world[2].xyz;
    output.normal = normalize(input.normal.x * cross(axisY, axisZ) + input.normal.y * cross(axisZ, axisX) +
                              input.normal.z * cross(axisX, axisY));
    // A tangent lies in the surface, so `world` itself is right for it
    output.tangent = normalize(mul(float4(input.tangent, 0.0f), world).xyz);
    output.worldPosition = worldPosition.xyz;
    return output;
}
