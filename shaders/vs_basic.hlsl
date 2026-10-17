// cube vertex shader.
// Matrix convention: CPU transposes (DirectXMath row-vector), HLSL uses mul(v, M).

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"

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
VSOutput main(VSInput input)
{
    VSOutput output;
    // The cube sits at the origin with no model matrix until per-object transforms arrive
    // (instance data, step 2)
    output.position = mul(float4(input.position, 1.0f), g_frame.viewProjection);
    output.color = float4(input.color, 1.0f);
    return output;
}
