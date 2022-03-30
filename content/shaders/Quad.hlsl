#include "Common.hlsli"

QUAD_VS_OUT VSMain(QUAD_VS_IN input)
{
    QUAD_VS_OUT output;

    output.Position = float4(input.Position.xyz, 1.0f);
    output.TexCoord = input.TexCoord;

    return output;
}
