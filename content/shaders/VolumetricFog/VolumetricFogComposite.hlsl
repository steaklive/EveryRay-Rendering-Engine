// ================================================================================================
// Pixel shader for composite pass of volumetric fog
// For more info check VolumetricFogMain.hlsl
//
// Written by Gen Afanasev for 'EveryRay Rendering Engine', 2017-2022
// ================================================================================================

#include "..\\Common.hlsli"
#include "VolumetricFog.hlsli"

Texture2D<float4> InputScreenColor : register(t0);
Texture2D<float4> GBufferWorldPosTexture : register(t1);
Texture3D<float4> VolumetricFogVoxelGridTexture : register(t2);

SamplerState SamplerLinear : register(s0);

cbuffer VolumetricFogCompositeCBuffer : register(b0)
{
    float4x4 ViewProj;
    float4 CameraNearFarPlanes;
    float4 VoxelSize;
    float BlendingWithSceneColorFactor;
}

float3 GetVolumetricFog(float3 inputColor, float3 worldPos, float nearPlane, float farPlane, float4x4 viewProj)
{
    float3 uv = GetUVFromVolumetricFogVoxelWorldPos(worldPos, nearPlane, farPlane, viewProj, VoxelSize.xyz);
    float4 scatteredLight = VolumetricFogVoxelGridTexture.SampleLevel(SamplerLinear, uv, 0.0f);
    return inputColor * scatteredLight.a + scatteredLight.rgb;
}

float4 PSComposite(float4 pos : SV_Position, float2 tex : TEXCOORD0) : SV_Target
{
    float4 res = float4(0.0, 0.0, 0.0, 1.0);
    float4 inputColor = InputScreenColor.Sample(SamplerLinear, tex);
    float4 worldPos = GBufferWorldPosTexture.Sample(SamplerLinear, tex);
    if (worldPos.w == 0.0f)
        return inputColor;
    
    float3 color = GetVolumetricFog(inputColor.rgb, worldPos.rgb, CameraNearFarPlanes.x, CameraNearFarPlanes.y, ViewProj);
    return float4(lerp(inputColor.rgb, color, BlendingWithSceneColorFactor), 1.0f);
}