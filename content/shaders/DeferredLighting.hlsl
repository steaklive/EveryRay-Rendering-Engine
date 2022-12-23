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
#include "Common.hlsli"

SamplerState SamplerLinear : register(s0);
SamplerComparisonState CascadedPcfShadowMapSampler : register(s1);
SamplerState SamplerClamp: register(s2);

RWTexture2D<float4> OutputTexture : register(u0);

Texture2D<float4> GbufferAlbedoTexture : register(t0);
Texture2D<float4> GbufferNormalTexture : register(t1);
Texture2D<float4> GbufferWorldPosTexture : register(t2);
Texture2D<float4> GbufferExtraTexture : register(t3); // [reflection mask, roughness, metalness, foliage mask]
Texture2D<float4> GbufferExtra2Texture : register(t4); // [global diffuse probe mask, POM, SSS, skip deferred lighting]

Texture2D<float> CascadedShadowTextures[NUM_OF_SHADOW_CASCADES] : register(t5);

cbuffer DeferredLightingCBuffer : register(b0)
{
    float4x4 ShadowMatrices[NUM_OF_SHADOW_CASCADES];
    float4x4 ViewProj;
    float4 ShadowCascadeDistances;
    float4 ShadowTexelSize;
    float4 SunDirection;
    float4 SunColor;
    float4 CameraPosition;
    float4 CameraNearFarPlanes;
    float UseGlobalProbe;
    float SkipIndirectLighting;
    float SSSTranslucency;
    float SSSWidth;
    float SSSDirectionLightMaxPlane;
    float SSSAvailable;
}

cbuffer LightProbesCBuffer : register(b1)
{
    float4 DiffuseProbesCellsCount;
    float4 SpecularProbesCellsCount; //x,y,z,total
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
    bool useSSS = extra2Gbuffer.b > -1.0f && SSSAvailable > 0.0f;
    bool isFoliage = extraGbuffer.a >= 1.0f;
    
    //reflectance at normal incidence for dia-electic or metal
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, diffuseAlbedo.rgb, metalness);

    float3 directLighting = float3(0.0, 0.0, 0.0);
    if (isFoliage)
        directLighting = SunColor.rgb * diffuseAlbedo.rgb * max(dot(normalWS, SunDirection.xyz), 0.0);
    else
        directLighting = DirectLightingPBR(normalWS, SunColor, SunDirection.xyz, diffuseAlbedo.rgb, worldPos.rgb, roughness, F0, metalness, CameraPosition.xyz);
    
    if (useSSS)
    {
        directLighting += SunColor.rgb * diffuseAlbedo.rgb * 
            SSSSTransmittance(SSSTranslucency, SSSWidth, worldPos.rgb, normalWS, SunDirection.xyz, ShadowMatrices[0], CascadedPcfShadowMapSampler, ShadowTexelSize.x, CascadedShadowTextures[0], SSSDirectionLightMaxPlane);        
    }
    
    float3 indirectLighting = float3(0.0, 0.0, 0.0);
    if (isFoliage)
        indirectLighting = float3(0.1f, 0.1f, 0.1f) * diffuseAlbedo.rgb; // fake ambient for foliage
    
    if (!isFoliage && SkipIndirectLighting <= 0.0f)
    {
        LightProbeInfo probesInfo;      
        probesInfo.globalIrradianceDiffuseProbeTexture = DiffuseGlobalProbeTexture;
        probesInfo.globalIrradianceSpecularProbeTexture = SpecularGlobalProbeTexture;

        probesInfo.DiffuseProbesCellsWithProbeIndicesArray = DiffuseProbesCellsWithProbeIndicesArray;
        probesInfo.DiffuseSphericalHarmonicsCoefficientsArray = DiffuseSphericalHarmonicsCoefficientsArray;
        probesInfo.DiffuseProbesPositionsArray = DiffuseProbesPositionsArray;

        probesInfo.SpecularProbesTextureArray = SpecularProbesTextureArray;
        probesInfo.SpecularProbesCellsWithProbeIndicesArray = SpecularProbesCellsWithProbeIndicesArray;
        probesInfo.SpecularProbesTextureArrayIndices = SpecularProbesTextureArrayIndices;
        probesInfo.SpecularProbesPositionsArray = SpecularProbesPositionsArray;
        
        probesInfo.diffuseProbeCellsCount = DiffuseProbesCellsCount;
        probesInfo.specularProbeCellsCount = SpecularProbesCellsCount;
        probesInfo.sceneLightProbeBounds = SceneLightProbesBounds;
        probesInfo.distanceBetweenDiffuseProbes = DistanceBetweenDiffuseProbes;
        probesInfo.distanceBetweenSpecularProbes = DistanceBetweenSpecularProbes;
        
        indirectLighting += IndirectLightingPBR(normalWS, diffuseAlbedo.rgb, worldPos.rgb, roughness, F0, metalness, CameraPosition.xyz, useGlobalProbe > 0.0f,
            probesInfo, SamplerLinear, SamplerClamp, IntegrationTexture, ao);
    }
    
    float shadow = Deferred_GetShadow(worldPos, ShadowMatrices, ShadowCascadeDistances, ShadowTexelSize.x, CascadedShadowTextures, CascadedPcfShadowMapSampler);
    
    float3 color = (directLighting * shadow) + indirectLighting;
    OutputTexture[inPos] += float4(color, 1.0f);
}