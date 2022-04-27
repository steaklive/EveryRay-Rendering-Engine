#include "..\\Common.hlsli"

Texture2D<float4> InputScreenColor : register(t0);
Texture2D<float4> GBufferWorldPosTexture : register(t1);
Texture3D<float4> VolumetricFogVoxelGridTexture : register(t2);

SamplerState SamplerLinear : register(s0);

cbuffer VolumetricFogCompositeCBuffer : register(b0)
{
    float4x4 ViewProj;
    float4 CameraNearFarPlanes;
}

float4 PSComposite(float4 pos : SV_Position, float2 tex : TEXCOORD0) : SV_Target
{
    float4 res = float4(0.0, 0.0, 0.0, 1.0);
    float4 color = InputScreenColor.Sample(SamplerLinear, tex);
    float4 worldPos = GBufferWorldPosTexture.Sample(SamplerLinear, tex);
    color.rgb = AddVolumetricFog(color.rgb, worldPos.rgb, CameraNearFarPlanes.x, CameraNearFarPlanes.y, ViewProj, SamplerLinear, VolumetricFogVoxelGridTexture);
    return float4(color.rgb, 1.0f);
}