// Unlit mesh pixel shader (port of the DX11 ps_nolight.hlsl): the albedo texture's colour, no lighting.

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"
#include "common/vs_output.hlsli"

// Root constants b0 of the global root signature, set before every draw
ConstantBuffer<DrawConstants> g_draw : register(b0);
// Static sampler s0 of the global root signature: trilinear, wrap
SamplerState g_linearWrap : register(s0);

[RootSignature(GLOBAL_ROOT_SIGNATURE)]
float4 main(VSOutput input) : SV_Target
{
    // Bindless (SM 6.6): until MaterialGPU (step 6) the material index is the albedo texture's slot.
    // It comes from root constants, so it is the same for the whole draw: no NonUniformResourceIndex needed.
    Texture2D<float4> albedo = ResourceDescriptorHeap[g_draw.materialIndex];
    // An _SRGB texture is decoded to linear when sampled; the sRGB render target encodes it again
    const float3 color = albedo.Sample(g_linearWrap, input.uv).rgb;
    return float4(color, 1.0f);
}
