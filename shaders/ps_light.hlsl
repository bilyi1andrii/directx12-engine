// Lit pixel shader: the texture's colour under Blinn-Phong lighting from the frame's lights (ambient, one
// directional light, point lights and spot lights). No shadows yet.
// Lighting is computed in linear space; bright areas clip to white until there is a tonemapper.

#include "common/root_signature.hlsli"
#include "common/shared_types.hlsli"
#include "common/vs_output.hlsli"

// Per-draw root constants (b0)
ConstantBuffer<DrawConstants> g_draw : register(b0);
// Per-frame constants (b1): the camera and the lights
ConstantBuffer<FrameConstants> g_frame : register(b1);
// Static sampler s0: anisotropic filtering, wrap
SamplerState g_anisotropicWrap : register(s0);

// What one light adds to a surface, before the light's colour and strength: the diffuse part plus the highlight.
// N: surface normal, L: towards the light, V: towards the camera, all unit length.
float3 DiffuseAndSpecular(float3 N, float3 L, float3 V)
{
    const float ndotl = dot(N, L);
    // A surface turned away from the light gets nothing from it, the highlight included
    if (ndotl <= 0.0f)
    {
        return 0.0f;
    }
    // Blinn-Phong: the highlight is strongest where the normal lies halfway between the light and the camera
    const float3 H = normalize(L + V);
    const float specular = pow(max(dot(N, H), 0.0f), g_frame.shininess);
    return ndotl + specular * g_frame.specularColor;
}

// How much of a light is left at this distance from it
float Attenuation(float3 attenuation, float distanceToLight)
{
    const float denominator = attenuation.x + attenuation.y * distanceToLight +
                              attenuation.z * distanceToLight * distanceToLight;
    return 1.0f / max(denominator, 1e-4f);
}

float3 DirectionalLight(float3 N, float3 V)
{
    // directionalDirection is where the light shines; -direction points back to it
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

    // Cosine of the angle between the cone's axis and the ray to this pixel: 1 on the axis
    const float theta = dot(light.direction, -L);
    // 1 inside the inner cone, 0 outside the outer one, a soft edge in between. The CPU keeps innerCos > outerCos.
    const float cone = saturate((theta - light.outerCos) / (light.innerCos - light.outerCos));

    return Attenuation(light.attenuation, distanceToLight) * cone * light.color * light.strength *
           DiffuseAndSpecular(N, L, V);
}

[RootSignature(GLOBAL_ROOT_SIGNATURE)]
float4 main(VSOutput input, bool isFrontFace : SV_IsFrontFace) : SV_Target
{
    // For now the material index is the bindless slot of the albedo texture
    Texture2D<float4> albedoTexture = ResourceDescriptorHeap[g_draw.materialIndex];
    // sRGB textures come back linear; the sRGB render target converts back on write
    const float3 albedo = albedoTexture.Sample(g_anisotropicWrap, input.uv).rgb;

    // Interpolating between vertices shortens the normal. Seen from behind (two-sided materials), the surface
    // faces the other way.
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

    return float4(albedo * lighting, 1.0f);
}
