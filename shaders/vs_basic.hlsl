// cube vertex shader.
// Matrix convention: CPU transposes (DirectXMath row-vector), HLSL uses mul(v, M).

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"

// Root constants b0 of the global root signature, set before every draw
ConstantBuffer<DrawConstants> g_draw : register(b0);
// Root CBV b1 of the global root signature, written once per frame from the UploadRing
ConstantBuffer<FrameConstants> g_frame : register(b1);

struct VSInput
{
    float3 position : POSITION;
    float3 color    : COLOR;
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 color    : COLOR;
};

[RootSignature(GLOBAL_ROOT_SIGNATURE)]
VSOutput main(VSInput input, uint instanceId : SV_InstanceID)
{
    // Bindless (SM 6.6): the frame's instance buffer is picked by its index in the descriptor heap.
    // SV_InstanceID starts at 0 in every draw, so the draw's first element comes from b0.
    StructuredBuffer<InstanceData> instances = ResourceDescriptorHeap[g_frame.instanceBufferIndex];
    const float4x4 world = instances[g_draw.instanceOffset + instanceId].world;

    VSOutput output;
    // Same math as the DX11 vs_instanced.hlsl / vs_not_instanced.hlsl
    const float4 worldPosition = mul(float4(input.position, 1.0f), world);
    output.position = mul(worldPosition, g_frame.viewProjection);
    output.color = float4(input.color, 1.0f);
    return output;
}
