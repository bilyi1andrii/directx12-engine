// Draws a surface in its texture's colour, with no lighting.

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"
#include "common/vs_output.hlsli"

ConstantBuffer<DrawConstants> g_draw : register(b0);
SamplerState g_anisotropicWrap : register(s0);

[RootSignature(GLOBAL_ROOT_SIGNATURE)]
float4 main(VSOutput input) : SV_Target
{
    Texture2D<float4> albedo = ResourceDescriptorHeap[g_draw.materialIndex];
    const float3 color = albedo.Sample(g_anisotropicWrap, input.uv).rgb;
    return float4(color, 1.0f);
}
