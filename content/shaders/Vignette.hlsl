#include "Common.hlsli"

Texture2D<float4> ColorTexture : register(t0);

cbuffer VignetteCBuffer : register(b0)
{
    float2 RadiusSoftness;
}

SamplerState LinearSampler : register(s0);

float4 PSMain(QUAD_VS_OUT IN) : SV_Target
{
    float4 color = ColorTexture.Sample(LinearSampler, IN.TexCoord);
	
    float len = distance(IN.TexCoord, float2(0.5, 0.5)) * 0.7f;
    float vignette = smoothstep(RadiusSoftness.r, RadiusSoftness.r - RadiusSoftness.g, len);
    color.rgb *= vignette;
    return color;
}