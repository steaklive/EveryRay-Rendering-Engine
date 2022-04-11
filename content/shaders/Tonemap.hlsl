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

float4 PSMain(QUAD_VS_OUT IN) : SV_Target
{
    float3 hdrColor = ColorTexture.Sample(LinearSampler, IN.TexCoord).rgb;
    float3 sdrColor = TM_Stanard(hdrColor);   
    return float4(ApplySRGBCurve(sdrColor), 1.0f);
}