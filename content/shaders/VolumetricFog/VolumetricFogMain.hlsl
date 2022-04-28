#include "..\\Common.hlsli"

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
    float4x4 ShadowMatrix;
    float4 SunDirection;
    float4 SunColor;
    float4 CameraPosition;
    float4 CameraNearFar;
    float Anisotropy;
    float Density;
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

float3 GetWorldPosFromVoxelID(uint3 texCoord, float jitter, float near, float far)
{
    float viewZ = near * pow(far / near, (float(texCoord.z) + 0.5f + jitter) / float(VOLUMETRIC_FOG_VOXEL_SIZE_Z));
    float3 uv = float3((float(texCoord.x) + 0.5f) / float(VOLUMETRIC_FOG_VOXEL_SIZE_X), (float(texCoord.y) + 0.5f) / float(VOLUMETRIC_FOG_VOXEL_SIZE_Y), viewZ / far);
    
    float3 ndc;
    ndc.x = 2.0f * uv.x - 1.0f;
    ndc.y = 1.0f - 2.0f * uv.y; //turn upside down for DX
    ndc.z = 2.0f * LinearToExponentialDepth(uv.z, near, far) - 1.0f;
    
    float4 worldPos = mul(float4(ndc, 1.0f), InvViewProj);
    worldPos = worldPos / worldPos.w;
    return worldPos.rgb;
}

[numthreads(8, 8, 1)]
void CSInjection(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    uint3 texCoord = DTid.xyz;
    
    if (texCoord.x < VOLUMETRIC_FOG_VOXEL_SIZE_X && texCoord.y < VOLUMETRIC_FOG_VOXEL_SIZE_Y && texCoord.z < VOLUMETRIC_FOG_VOXEL_SIZE_Z)
    {
        float jitter = (GetBlueNoiseSample(texCoord) - 0.5f) * (1.0f - EPSILON);
        float3 voxelWorldPos = GetWorldPosFromVoxelID(texCoord, jitter, CameraNearFar.x, CameraNearFar.y);
        float3 viewDir = normalize(CameraPosition.xyz - voxelWorldPos);

        float3 lighting = SunColor.rgb * SunColor.a;
        float visibility = GetVisibility(voxelWorldPos, ShadowMatrix);

        if (visibility > EPSILON)
            lighting += visibility * SunColor.xyz * HenyeyGreensteinPhaseFunction(viewDir, -SunDirection.xyz, Anisotropy);

        float4 result = float4(lighting * Density, Density);
        VoxelWriteTexture[texCoord] = result;
    }

}
float GetSliceDistance(int z, float near, float far)
{
    return near * pow(far / near, (float(z) + 0.5f) / float(VOLUMETRIC_FOG_VOXEL_SIZE_Z));
}
float GetSliceThickness(int z, float near, float far)
{
    return abs(GetSliceDistance(z + 1, near, far) - GetSliceDistance(z, near, far));
}

// https://github.com/Unity-Technologies/VolumetricLighting/blob/master/Assets/VolumetricFog/Shaders/Scatter.compute
float4 Accumulate(int z, float4 result /*color (rgb) & transmittance (alpha)*/, float4 colorDensityPerSlice /*color (rgb) & density (alpha)*/)
{
    float thickness = GetSliceThickness(z, CameraNearFar.x, CameraNearFar.y);
    float sliceTransmittance = exp(-colorDensityPerSlice.a * thickness * 0.01f);

    float3 sliceScattering = colorDensityPerSlice.rgb * (1.0f - sliceTransmittance) / colorDensityPerSlice.a;
    result.rgb += sliceScattering * result.a;
    result.a *= sliceTransmittance;
    return result;
}

[numthreads(8, 8, 1)]
void CSAccumulation(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    float4 result = float4(0.0f, 0.0f, 0.0f, 1.0f);

    for (int z = 0; z < VOLUMETRIC_FOG_VOXEL_SIZE_Z; z++)
    {
        uint3 texCoord = uint3(DTid.xy, z);
        float4 colorDensityPerSlice = VoxelReadTexture.Load(uint4(texCoord, 0));

        result = Accumulate(z, result, colorDensityPerSlice);
        VoxelWriteTexture[texCoord] = result;
    }
}