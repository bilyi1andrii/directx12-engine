// What the mesh vertex shader hands the pixel shaders, one struct for both stages.

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
