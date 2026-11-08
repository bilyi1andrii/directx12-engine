// Unlit pixel shader: the texture's colour, no lighting.

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"
#include "common/vs_output.hlsli"

// Per-draw root constants (b0)
ConstantBuffer<DrawConstants> g_draw : register(b0);
// Static sampler s0: linear filtering, wrap
SamplerState g_linearWrap : register(s0);

[RootSignature(GLOBAL_ROOT_SIGNATURE)]
float4 main(VSOutput input) : SV_Target
{
    // For now the material index is the bindless slot of the texture
    Texture2D<float4> albedo = ResourceDescriptorHeap[g_draw.materialIndex];
    // sRGB textures come back linear; the sRGB render target converts back on write
    const float3 color = albedo.Sample(g_linearWrap, input.uv).rgb;
    return float4(color, 1.0f);
}
