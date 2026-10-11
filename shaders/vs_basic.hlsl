// cube vertex shader.
// Matrix convention: CPU transposes (DirectXMath row-vector), HLSL uses mul(v, M).

struct DrawConstants
{
    float4x4 mvp;
};
ConstantBuffer<DrawConstants> g_draw : register(b0);

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

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.position, 1.0f), g_draw.mvp);
    output.color = float4(input.color, 1.0f);
    return output;
}
