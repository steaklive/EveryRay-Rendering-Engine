// ================================================================================================
// Volumetric fog based on
// "Volumetric fog: Unified, compute shader based solution to atmospheric scattering" by B.Wronski
// 
// Some parts of the code are borrowed from:
// https://github.com/diharaw/volumetric-lighting
// https://github.com/Unity-Technologies/VolumetricLighting
//
// The systems performs 2 compute passes: light injection and scattering
//
// Supports:
// - Directional Light
// - Previous frame interpolation
//
// TODO:
// - add support for point/spot lights
// - use optimized shadow map (downsample)
//
// Written by Gen Afanasev for 'EveryRay Rendering Engine', 2017-2022
// ================================================================================================

#include "..\\Common.hlsli"
#include "VolumetricFog.hlsli"

#define EPSILON 0.000001

SamplerState SamplerLinear : register(s0);
SamplerComparisonState CascadedPcfShadowMapSampler : register(s1);

RWTexture3D<float4> VoxelWriteTexture : register(u0);

Texture2D<float> ShadowTexture : register(t0);
Texture2D<float4> BlueNoiseTexture : register(t1);
Texture3D<float4> VoxelReadTexture : register(t2);

cbuffer VolumetricFogCBuffer : register(b0)
{
    float4x4 InvViewProj;
    float4x4 PrevViewProj;
    float4x4 ShadowMatrix;
    float4 SunDirection;
    float4 SunColor;
    float4 CameraPosition;
    float4 CameraNearFar_FrameIndex_PreviousFrameBlend;
    float4 VolumeSize;
    float Anisotropy;
    float Density;
    float Strength;
    float ThicknessFactor;
}
float HenyeyGreensteinPhaseFunction(float3 viewDir, float3 lightDir, float g)
{
    float cos_theta = dot(viewDir, lightDir);
    float denom = 1.0f + g * g + 2.0f * g * cos_theta;
    return (1.0f / (4.0f * PI)) * (1.0f - g * g) / max(pow(denom, 1.5f), EPSILON);
}

float GetBlueNoiseSample(uint3 texCoord)
{
    uint width, height;
    BlueNoiseTexture.GetDimensions(width, height);
    uint2 noiseCoord = (texCoord.xy + uint2(0, 1) * texCoord.z * width) % width;
    return BlueNoiseTexture.Load(uint3(noiseCoord, 0)).r;
}
float GetVisibility(float3 voxelWorldPoint, float4x4 svp)
{
    float4 lightSpacePos = mul(svp, float4(voxelWorldPoint, 1.0f));
    float4 ShadowCoord = lightSpacePos / lightSpacePos.w;
    ShadowCoord.rg = ShadowCoord.rg * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    
    return ShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy, ShadowCoord.z).r;
}

[numthreads(8, 8, 1)]
void CSInjection(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    uint3 texCoord = DTid.xyz;
    
    if (texCoord.x < VolumeSize.x && texCoord.y < VolumeSize.y && texCoord.z < VolumeSize.z)
    {
        float jitter = frac((GetBlueNoiseSample(texCoord) - 0.5f) * (1.0f - EPSILON) * CameraNearFar_FrameIndex_PreviousFrameBlend.z);
        float3 voxelWorldPos = GetWorldPosFromVoxelID(texCoord, jitter, CameraNearFar_FrameIndex_PreviousFrameBlend.x, CameraNearFar_FrameIndex_PreviousFrameBlend.y, InvViewProj, VolumeSize.xyz);
        float3 voxelWorldPosNoJitter = GetWorldPosFromVoxelID(texCoord, 0.0f, CameraNearFar_FrameIndex_PreviousFrameBlend.x, CameraNearFar_FrameIndex_PreviousFrameBlend.y, InvViewProj, VolumeSize.xyz);
        float3 viewDir = normalize(CameraPosition.xyz - voxelWorldPos);

        float3 lighting = float3(0.0, 0.0, 0.0);
        float visibility = GetVisibility(voxelWorldPos, ShadowMatrix);
        //float visibility2 = GetVisibility(voxelWorldPosNoJitter, ShadowMatrix);

        if (visibility > EPSILON)
            lighting += visibility * SunColor.xyz * HenyeyGreensteinPhaseFunction(viewDir, -SunDirection.xyz, Anisotropy);
        
        float4 result = float4(lighting * Strength * Density, visibility * Density);
        
        //previous frame interpolation
        {
            float3 prevUV = GetUVFromVolumetricFogVoxelWorldPos(voxelWorldPosNoJitter, 
            CameraNearFar_FrameIndex_PreviousFrameBlend.x, CameraNearFar_FrameIndex_PreviousFrameBlend.y, PrevViewProj, VolumeSize.xyz);
            
            if (prevUV.x >= 0.0f && prevUV.y >= 0.0f && prevUV.z >= 0.0f &&
                prevUV.x <= 1.0f && prevUV.y <= 1.0f && prevUV.z <= 1.0f)
            {
                float4 prevResult = VoxelReadTexture.SampleLevel(SamplerLinear, prevUV, 0.0f);
                result = lerp(result, prevResult, CameraNearFar_FrameIndex_PreviousFrameBlend.w);
            }
        }

        VoxelWriteTexture[texCoord] = result;
    }
}

float GetSliceDistance(int z, float near, float far)
{
    return near * pow(far / near, (float(z) + 0.5f) / VolumeSize.z);
}
float GetSliceThickness(int z, float near, float far)
{
    return abs(GetSliceDistance(z + 1, near, far) - GetSliceDistance(z, near, far));
}

// https://github.com/Unity-Technologies/VolumetricLighting/blob/master/Assets/VolumetricFog/Shaders/Scatter.compute
float4 Accumulate(int z, float4 result /*color (rgb) & transmittance (alpha)*/, float4 colorDensityPerSlice /*color (rgb) & density (alpha)*/)
{
    colorDensityPerSlice.a = max(colorDensityPerSlice.a, 0.000001);
    //float thickness = GetSliceThickness(z, CameraNearFar_FrameIndex_PreviousFrameBlend.x, CameraNearFar_FrameIndex_PreviousFrameBlend.y);
    //float sliceTransmittance = exp(-colorDensityPerSlice.a * thickness * ThicknessFactor);

    // Seb Hillaire's improved transmission by calculating an integral over slice depth instead of
	// constant per slice value. Light still constant per slice, but that's acceptable. See slide 28 of
	// Physically-based & Unified Volumetric Rendering in Frostbite
	// http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/
    float sliceTransmittance = exp(-colorDensityPerSlice.a / VolumeSize.z);

    float3 sliceScattering = colorDensityPerSlice.rgb * (1.0f - sliceTransmittance) / colorDensityPerSlice.a;

    result.rgb += sliceScattering * result.a;
    result.a *= sliceTransmittance;
    return result;
}

[numthreads(8, 8, 1)]
void CSAccumulation(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    float4 result = float4(0.0f, 0.0f, 0.0f, 1.0f);

    for (int z = 0; z < VolumeSize.z; z++)
    {
        uint3 texCoord = uint3(DTid.xy, z);
        float4 colorDensityPerSlice = VoxelReadTexture.Load(uint4(texCoord, 0));

        result = Accumulate(z, result, colorDensityPerSlice);
        VoxelWriteTexture[texCoord] = result;
    }
}