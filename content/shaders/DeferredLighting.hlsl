// ================================================================================================
// Compute shader for classic "Deferred" lighting. 
// 
// Supports:
// - Cascaded Shadow Mapping
// - PBR with Image Based Lighting (via light probes)
//
// TODO:
// - move to "Tiled/Clustered Deferred"
// - add support for point/spot lights
// - add support for ambient occlusion
//
// Written by Gen Afanasev for 'EveryRay Rendering Engine', 2017-2022
// ================================================================================================

#include "Lighting.hlsli"

SamplerState SamplerLinear : register(s0);
SamplerComparisonState CascadedPcfShadowMapSampler : register(s1);

RWTexture2D<float4> OutputTexture : register(u0);

Texture2D<float4> GbufferAlbedoTexture : register(t0);
Texture2D<float4> GbufferNormalTexture : register(t1);
Texture2D<float4> GbufferWorldPosTexture : register(t2);
Texture2D<float4> GbufferExtraTexture : register(t3); // [reflection mask, roughness, metalness, foliage mask]
Texture2D<float4> GbufferExtra2Texture : register(t4); // [global diffuse probe mask, POM, empty, skip deferred lighting]

Texture2D<float> CascadedShadowTextures[NUM_OF_SHADOW_CASCADES] : register(t5);

cbuffer DeferredLightingCBuffer : register(b0)
{
    float4x4 ShadowMatrices[NUM_OF_SHADOW_CASCADES];
    float4 ShadowCascadeDistances;
    float4 ShadowTexelSize;
    float4 SunDirection;
    float4 SunColor;
    float4 CameraPosition;
    float UseGlobalProbe;
    float SkipIndirectLighting;
}

cbuffer LightProbesCBuffer : register(b1)
{
    float4 DiffuseProbesCellsCount[NUM_OF_PROBE_VOLUME_CASCADES]; //x,y,z,total
    float4 DiffuseProbesVolumeSizes[NUM_OF_PROBE_VOLUME_CASCADES];
    float4 SpecularProbesCellsCount[NUM_OF_PROBE_VOLUME_CASCADES]; //x,y,z,total
    float4 SpecularProbesVolumeSizes[NUM_OF_PROBE_VOLUME_CASCADES];
    float4 VolumeProbesIndexSkips[NUM_OF_PROBE_VOLUME_CASCADES];
    float4 SceneLightProbesBounds; //volume's extent of all scene's probes
    float DistanceBetweenDiffuseProbes;
    float DistanceBetweenSpecularProbes;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    uint2 inPos = DTid.xy;
    
    float4 extra2Gbuffer = GbufferExtra2Texture.Load(uint3(inPos, 0));
    bool skipLighting = extra2Gbuffer.a > 0.0f;
    if (skipLighting)
    {
        OutputTexture[inPos] += float4(0.0, 0.0, 0.0, 0.0f);
        return;
    }
    
    bool useGlobalProbe = (UseGlobalProbe > 0.0f) || (extra2Gbuffer.r > 0.0f);
    
    float4 worldPos = GbufferWorldPosTexture.Load(uint3(inPos, 0));
    if (worldPos.a < 0.000001f)
        return;
    
    float4 diffuseAlbedoNonGamma = GbufferAlbedoTexture.Load(uint3(inPos, 0));
    if(diffuseAlbedoNonGamma.a < 0.000001f)
        return;
    float3 diffuseAlbedo = pow(diffuseAlbedoNonGamma.rgb, 2.2);
    
    float3 normalWS = normalize(GbufferNormalTexture.Load(uint3(inPos, 0)).rgb);
    
    float4 extraGbuffer = GbufferExtraTexture.Load(uint3(inPos, 0));
    float roughness = extraGbuffer.g;
    float metalness = extraGbuffer.b;
    
    float ao = 1.0f; // TODO sample AO texture
    
    bool usePOM = extra2Gbuffer.g > -1.0f; // TODO add POM support to Deferred

    //reflectance at normal incidence for dia-electic or metal
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, diffuseAlbedo.rgb, metalness);

    float3 directLighting = DirectLightingPBR(normalWS, SunColor, SunDirection.xyz, diffuseAlbedo.rgb, worldPos.rgb, roughness, F0, metalness, CameraPosition.xyz);
    
    float3 indirectLighting = float3(0.0, 0.0, 0.0);
    if (extraGbuffer.a < 1.0f && SkipIndirectLighting <= 0.0f)
    {
        LightProbeInfo probesInfo;
        probesInfo.globalIrradianceDiffuseProbeTexture = IrradianceDiffuseGlobalProbeTexture;
        probesInfo.globalIrradianceSpecularProbeTexture = IrradianceSpecularGlobalProbeTexture;
        probesInfo.IrradianceDiffuseProbesTextureArray0 = IrradianceDiffuseProbesTextureArray0;
        probesInfo.IrradianceDiffuseProbesTextureArray1 = IrradianceDiffuseProbesTextureArray1;
        probesInfo.IrradianceSpecularProbesTextureArray0 = IrradianceSpecularProbesTextureArray0;
        probesInfo.IrradianceSpecularProbesTextureArray1 = IrradianceSpecularProbesTextureArray1;
        probesInfo.DiffuseProbesCellsWithProbeIndices0 = DiffuseProbesCellsWithProbeIndices0;
        probesInfo.DiffuseProbesCellsWithProbeIndices1 = DiffuseProbesCellsWithProbeIndices1;
        probesInfo.DiffuseProbesTextureArrayIndices0 = DiffuseProbesTextureArrayIndices0;
        probesInfo.DiffuseProbesTextureArrayIndices1 = DiffuseProbesTextureArrayIndices1;
        probesInfo.DiffuseProbesPositions = DiffuseProbesPositions;
        probesInfo.SpecularProbesCellsWithProbeIndices0 = SpecularProbesCellsWithProbeIndices0;
        probesInfo.SpecularProbesCellsWithProbeIndices1 = SpecularProbesCellsWithProbeIndices1;
        probesInfo.SpecularProbesTextureArrayIndices0 = SpecularProbesTextureArrayIndices0;
        probesInfo.SpecularProbesTextureArrayIndices1 = SpecularProbesTextureArrayIndices1;
        probesInfo.SpecularProbesPositions = SpecularProbesPositions;
        probesInfo.diffuseProbesVolumeSizes = DiffuseProbesVolumeSizes;
        probesInfo.diffuseProbeCellsCount = DiffuseProbesCellsCount;
        probesInfo.specularProbesVolumeSizes = SpecularProbesVolumeSizes;
        probesInfo.specularProbeCellsCount = SpecularProbesCellsCount;
        probesInfo.volumeProbesIndexSkips = VolumeProbesIndexSkips;
        probesInfo.sceneLightProbeBounds = SceneLightProbesBounds;
        probesInfo.distanceBetweenDiffuseProbes = DistanceBetweenDiffuseProbes;
        probesInfo.distanceBetweenSpecularProbes = DistanceBetweenSpecularProbes;
        
        indirectLighting += IndirectLightingPBR(normalWS, diffuseAlbedo.rgb, worldPos.rgb, roughness, F0, metalness, CameraPosition.xyz, useGlobalProbe > 0.0f,
            probesInfo, SamplerLinear, IntegrationTexture, ao);
    }
    
    float shadow = Deferred_GetShadow(worldPos, ShadowMatrices, ShadowCascadeDistances, ShadowTexelSize.x, CascadedShadowTextures, CascadedPcfShadowMapSampler);
    
    float3 color = (directLighting * shadow) + indirectLighting;
    OutputTexture[inPos] += float4(color, 1.0f);
}