// The pixel shader for lit surfaces: the texture's colour, lit by the ambient light, the sun-like light and the
// point and spot lights. Two more builds of it draw cut-out surfaces (with holes) and see-through ones (blended).

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"
#include "common/vs_output.hlsli"

ConstantBuffer<DrawConstants> g_draw : register(b0);
ConstantBuffer<FrameConstants> g_frame : register(b1);
SamplerState g_anisotropicWrap : register(s0);

// N: surface normal, L: towards the light, V: towards the camera, all unit length
float3 DiffuseAndSpecular(float3 N, float3 L, float3 V)
{
    const float ndotl = dot(N, L);
    if (ndotl <= 0.0f)
    {
        return 0.0f;
    }
    // Blinn-Phong
    const float3 H = normalize(L + V);
    const float specular = pow(max(dot(N, H), 0.0f), g_frame.shininess);
    return ndotl + specular * g_frame.specularColor;
}

float Attenuation(float3 attenuation, float distanceToLight)
{
    const float denominator = attenuation.x + attenuation.y * distanceToLight +
                              attenuation.z * distanceToLight * distanceToLight;
    return 1.0f / max(denominator, 1e-4f);
}

float3 DirectionalLight(float3 N, float3 V)
{
    return g_frame.directionalColor * g_frame.directionalStrength *
           DiffuseAndSpecular(N, -g_frame.directionalDirection, V);
}

float3 PointLight(PointLightData light, float3 N, float3 worldPosition, float3 V)
{
    const float3 toLight = light.position - worldPosition;
    const float distanceToLight = length(toLight);
    const float3 L = toLight / max(distanceToLight, 1e-6f);
    return Attenuation(light.attenuation, distanceToLight) * light.color * light.strength *
           DiffuseAndSpecular(N, L, V);
}

float3 SpotLight(SpotLightData light, float3 N, float3 worldPosition, float3 V)
{
    const float3 toLight = light.position - worldPosition;
    const float distanceToLight = length(toLight);
    const float3 L = toLight / max(distanceToLight, 1e-6f);

    const float theta = dot(light.direction, -L);
    const float cone = saturate((theta - light.outerCos) / (light.innerCos - light.outerCos));

    return Attenuation(light.attenuation, distanceToLight) * cone * light.color * light.strength *
           DiffuseAndSpecular(N, L, V);
}

[RootSignature(GLOBAL_ROOT_SIGNATURE)]
float4 main(VSOutput input, bool isFrontFace : SV_IsFrontFace) : SV_Target
{
    Texture2D<float4> albedoTexture = ResourceDescriptorHeap[g_draw.materialIndex];
    const float4 albedoSample = albedoTexture.Sample(g_anisotropicWrap, input.uv);
    const float3 albedo = albedoSample.rgb;

#if defined(ALPHA_TEST) || defined(ALPHA_BLEND)
    Texture2D<float4> opacityTexture = ResourceDescriptorHeap[g_draw.alphaInfo >> 8];
    const float alpha = albedoSample.a * opacityTexture.Sample(g_anisotropicWrap, input.uv).r;
    const float alphaByte = (g_draw.alphaInfo & 0xFF) / 255.0f;
#endif
#ifdef ALPHA_TEST
    clip(alpha - alphaByte);
#endif

    float3 N = normalize(input.normal);
    N = isFrontFace ? N : -N;
    const float3 V = normalize(g_frame.cameraPosition - input.worldPosition);

    float3 lighting = g_frame.ambientColor * g_frame.ambientStrength;
    lighting += DirectionalLight(N, V);
    for (uint i = 0; i < g_frame.pointLightCount; ++i)
    {
        lighting += PointLight(g_frame.pointLights[i], N, input.worldPosition, V);
    }
    for (uint j = 0; j < g_frame.spotLightCount; ++j)
    {
        lighting += SpotLight(g_frame.spotLights[j], N, input.worldPosition, V);
    }

#ifdef ALPHA_BLEND
    return float4(albedo * lighting, alpha * alphaByte);
#else
    return float4(albedo * lighting, 1.0f);
#endif
}