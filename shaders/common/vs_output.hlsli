// Output of vs_mesh.hlsl and input of the mesh pixel shaders ("common/vs_output.hlsli").
// One struct for both stages, so their signatures cannot drift apart (in DX11, vs_instanced's VS_OUT had a
// tangent that ps_nolight's PS_IN lacked).

#ifndef VS_OUTPUT_HLSLI
#define VS_OUTPUT_HLSLI

struct VSOutput
{
    float4 position      : SV_Position;
    float2 uv            : TEXCOORD;
    float3 normal        : NORMAL;         // world space
    float3 tangent       : TANGENT;        // world space
    float3 worldPosition : WORLD_POSITION;
};

#endif // VS_OUTPUT_HLSLI
