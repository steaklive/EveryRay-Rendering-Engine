#include "Common.hlsli"

Texture2D<float4> ColorTexture : register(t0);
Texture2D<float> DepthTexture : register(t1);

cbuffer LinearFogCBuffer : register(b0)
{
    float4 FogColor;
    float FogNear;
    float FogFar;
    float FogDensity;
}

SamplerState LinearSampler : register(s0);

float linearizeDepth(float depth, float NearPlaneZ, float FarPlaneZ)
{
    float ProjectionA = FarPlaneZ / (FarPlaneZ - NearPlaneZ);
    float ProjectionB = (-FarPlaneZ * NearPlaneZ) / (FarPlaneZ - NearPlaneZ);

	// Sample the depth and convert to linear view space Z (assume it gets sampled as
	// a floating point value of the range [0,1])
    float linearDepth = ProjectionB / (depth - ProjectionA);
    return linearDepth;
}

float4 PSMain(QUAD_VS_OUT IN) : SV_Target
{
    float2 texCoord = IN.TexCoord;
    float4 color = ColorTexture.Sample(LinearSampler, texCoord);
    float depth = DepthTexture.Sample(LinearSampler, texCoord).r;
    
    if (depth > 0.9999f)
        return color;
    
    depth = linearizeDepth(depth, FogNear, FogFar);
    
    //float density = saturate(depth * FogDensity);
    float density = depth / FogDensity;
    return lerp(color, FogColor, density);
}