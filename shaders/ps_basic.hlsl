// cube pixel shader: outputs the interpolated vertex colour.

#include "common/root_signature.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float4 color    : COLOR;
};

[RootSignature(GLOBAL_ROOT_SIGNATURE)]
float4 main(PSInput input) : SV_Target
{
    return input.color;
}
