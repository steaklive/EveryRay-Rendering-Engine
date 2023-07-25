// ================================================================================================
// Simple tonemapping shader with Reinhard model
//
// Borrowed from Microsoft's MiniEngine:
// https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/MiniEngine
//
// Written by Gen Afanasev for 'EveryRay Rendering Engine', 2017-2022
// ================================================================================================

#include "Common.hlsli"
#include "Lighting.hlsli"

Texture2D<float4> ColorTexture : register(t0);
Texture2D<float> DepthTexture : register(t1); // for ignoring sky
SamplerState LinearSampler : register(s0);

float3 ApplySRGBCurve(float3 x)
{
    // Approximately pow(x, 1.0 / 2.2)
    return x < 0.0031308 ? 12.92 * x : 1.055 * pow(x, 1.0 / 2.4) - 0.055;
}
// The Reinhard tone operator.  Typically, the value of k is 1.0, but you can adjust exposure by 1/k.
// I.e. TM_Reinhard(x, 0.5) == TM_Reinhard(x * 2.0, 1.0)
float3 TM_Reinhard(float3 hdr, float k = 1.0)
{
    return hdr / (hdr + k);
}

float3 TM_Stanard(float3 hdr)
{
    return TM_Reinhard(hdr * sqrt(hdr), sqrt(4.0 / 27.0));
}

float3 ACESFilm(float3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

float4 PSMain(QUAD_VS_OUT IN) : SV_Target
{
    float3 hdrColor = ColorTexture.Sample(LinearSampler, IN.TexCoord).rgb;
    float depth = DepthTexture.Sample(LinearSampler, IN.TexCoord).r;
    if (depth < 1.0f - EPSILON)
    {
        float3 sdrColor = /*ACESFilm*/TM_Stanard(hdrColor);
        return float4(ApplySRGBCurve(sdrColor), 1.0f);
    }
    else
        return float4(hdrColor, 1.0f);
}