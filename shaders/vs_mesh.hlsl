// Mesh vertex shader (port of the DX11 vs_instanced.hlsl): position, UV, normal and tangent to world space.
// Matrix convention: CPU transposes (DirectXMath row-vector), HLSL uses mul(v, M).

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"
#include "common/vs_output.hlsli"

// Root constants b0 of the global root signature, set before every draw
ConstantBuffer<DrawConstants> g_draw : register(b0);
// Root CBV b1 of the global root signature, written once per frame from the UploadRing
ConstantBuffer<FrameConstants> g_frame : register(b1);

// Must match the C++ Vertex and the PSO's input layout (Renderer.cpp)
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
    // DX11 read the world matrix from a per-instance vertex stream; here it comes from the frame's instance
    // buffer (bindless), at the draw's first element (b0) + SV_InstanceID
    StructuredBuffer<InstanceData> instances = ResourceDescriptorHeap[g_frame.instanceBufferIndex];
    const float4x4 world = instances[g_draw.instanceOffset + instanceId].world;

    VSOutput output;
    const float4 worldPosition = mul(float4(input.position, 1.0f), world);
    output.position = mul(worldPosition, g_frame.viewProjection);
    output.uv = input.uv;
    // DX11 math: directions transformed by the world matrix itself, not its inverse transpose (wrong under
    // non-uniform scale). Fixed together with lighting in step 3.
    output.normal = normalize(mul(float4(input.normal, 0.0f), world).xyz);
    output.tangent = normalize(mul(float4(input.tangent, 0.0f), world).xyz);
    output.worldPosition = worldPosition.xyz;
    return output;
}
