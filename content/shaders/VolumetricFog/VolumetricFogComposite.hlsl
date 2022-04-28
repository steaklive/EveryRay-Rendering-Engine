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

float3 GetUVFromVolumetricFogVoxelWorldPos(float3 worldPos, float n, float f, float4x4 viewProj)
{
    float4 ndc = mul(float4(worldPos, 1.0f), viewProj);
    ndc = ndc / ndc.w;
    
    float3 uv;
    uv.x = ndc.x * 0.5f + 0.5f;
    uv.y = 0.5f - ndc.y * 0.5f; //turn upside down for DX
    uv.z = ExponentialToLinearDepth(ndc.z * 0.5f + 0.5f, n, f);
    
    float2 params = float2(float(VOLUMETRIC_FOG_VOXEL_SIZE_Z) / log2(f / n), -(float(VOLUMETRIC_FOG_VOXEL_SIZE_Z) * log2(n) / log2(f / n)));
    float view_z = uv.z * f;
    uv.z = (max(log2(view_z) * params.x + params.y, 0.0f)) / VOLUMETRIC_FOG_VOXEL_SIZE_Z;
    return uv;
}

float3 AddVolumetricFog(float3 inputColor, float3 worldPos, float nearPlane, float farPlane, float4x4 viewProj)
{
    float3 uv = GetUVFromVolumetricFogVoxelWorldPos(worldPos, nearPlane, farPlane, viewProj);
    float4 scatteredLight = VolumetricFogVoxelGridTexture.SampleLevel(SamplerLinear, uv, 0.0f);
    return inputColor * scatteredLight.a + scatteredLight.rgb;
}

float4 PSComposite(float4 pos : SV_Position, float2 tex : TEXCOORD0) : SV_Target
{
    float4 res = float4(0.0, 0.0, 0.0, 1.0);
    float4 color = InputScreenColor.Sample(SamplerLinear, tex);
    float4 worldPos = GBufferWorldPosTexture.Sample(SamplerLinear, tex);
    if (worldPos.w == 0.0f)
        return color;
    
    color.rgb = AddVolumetricFog(color.rgb, worldPos.rgb, CameraNearFarPlanes.x, CameraNearFarPlanes.y, ViewProj);
    return float4(color.rgb, 1.0f);
}