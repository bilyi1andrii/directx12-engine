// Reads a material's cut-out and see-through values from the per-draw constants: its opacity map, its cutoff and its
// opacity.

#ifndef ALPHA_TEST_HLSLI
#define ALPHA_TEST_HLSLI

float OpacityMapAlpha(uint alphaInfo, SamplerState opacitySampler, float2 uv)
{
    Texture2D<float4> opacityTexture = ResourceDescriptorHeap[alphaInfo >> 8];
    return opacityTexture.Sample(opacitySampler, uv).r;
}

float AlphaByte(uint alphaInfo)
{
    return (alphaInfo & 0xFF) / 255.0f;
}

#endif // ALPHA_TEST_HLSLI
