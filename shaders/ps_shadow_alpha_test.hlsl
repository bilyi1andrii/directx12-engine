// Lets light through a cut-out shadow caster where its alpha is below the material's cutoff. It writes only depth,
// so the rest of the caster casts shadow as usual.

#include "common/alpha_test.hlsli"
#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"
#include "common/vs_output.hlsli"

ConstantBuffer<DrawConstants> g_draw : register(b0);
SamplerState g_anisotropicWrap : register(s0);

[RootSignature(GLOBAL_ROOT_SIGNATURE)]
void main(VSOutput input)
{
    Texture2D<float4> albedoTexture = ResourceDescriptorHeap[g_draw.materialIndex];
    const float alpha = albedoTexture.Sample(g_anisotropicWrap, input.uv).a *
                        OpacityMapAlpha(g_draw.alphaInfo, g_anisotropicWrap, input.uv);
    clip(alpha - AlphaByte(g_draw.alphaInfo));
}
