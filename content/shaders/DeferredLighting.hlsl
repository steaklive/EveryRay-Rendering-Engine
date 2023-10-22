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
// Written by Gen Afanasev for 'EveryRay Rendering Engine', 2017-2023
// ================================================================================================

#include "Lighting.hlsli"
#include "Common.hlsli"

RWTexture2D<unorm float4> OutputTexture : register(u0);

Texture2D<float4> GbufferAlbedoTexture : register(t0);
Texture2D<float4> GbufferNormalTexture : register(t1);
Texture2D<float4> GbufferWorldPosTexture : register(t2);
Texture2D<float4> GbufferExtraTexture : register(t3); // [4 channels: extra mask value, roughness, metalness, height mask value]
Texture2D<uint> GbufferExtra2Texture : register(t4); // [1 channel: rendering object bitmask flags]

// WARNING: Check Lighting.hlsli or Common.hlsli before binding into a specific index!

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
    float SSSTranslucency;
    float SSSWidth;
    float SSSDirectionLightMaxPlane;
    float SSSAvailable;
    bool HasGlobalProbe;
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
    uint objectFlags = GbufferExtra2Texture.Load(uint3(inPos, 0)).r;

    if (objectFlags & RENDERING_OBJECT_FLAG_SKIP_DEFERRED_PASS)
    {
        OutputTexture[inPos] += float4(0.0, 0.0, 0.0, 0.0f);
        return;
    }
    
    bool useGlobalProbe = HasGlobalProbe || (objectFlags & RENDERING_OBJECT_FLAG_USE_GLOBAL_DIF_PROBE);
    
    float4 worldPos = GbufferWorldPosTexture.Load(uint3(inPos, 0));
    if (worldPos.a < 0.000001f)
        return;
    
    float4 diffuseAlbedoNonGamma = GbufferAlbedoTexture.Load(uint3(inPos, 0));
    if(diffuseAlbedoNonGamma.a < 0.000001f)
        return;
    float3 diffuseAlbedo = pow(diffuseAlbedoNonGamma.rgb, 2.2);
    
    float3 normalWS = normalize(GbufferNormalTexture.Load(uint3(inPos, 0)).rgb);
    
    float4 extraGbuffer = GbufferExtraTexture.Load(uint3(inPos, 0));
    float roughness = max(0.01, extraGbuffer.g);
    float metalness = extraGbuffer.b;
    float ao = 1.0f; // TODO add support for AO textures
    
    bool usePOM = (objectFlags & RENDERING_OBJECT_FLAG_POM); // TODO add POM support to Deferred
    bool useSSS = (objectFlags & RENDERING_OBJECT_FLAG_SSS) && SSSAvailable > 0.0f;
    bool isFoliage = (objectFlags & RENDERING_OBJECT_FLAG_FOLIAGE);
    bool skipIndirectSpecular = (objectFlags & RENDERING_OBJECT_FLAG_SKIP_INDIRECT_SPEC);

    //reflectance at normal incidence for dia-electic or metal
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, diffuseAlbedo.rgb, metalness);

    float3 directLighting = float3(0.0, 0.0, 0.0);
    if (isFoliage)
    {
        directLighting = SunColor.rgb * diffuseAlbedo.rgb * max(dot(normalWS, SunDirection.xyz), 0.0);
        // TODO: add other light sources contribution
    }
    else
    {
        directLighting = DirectLightingPBR(normalWS, SunColor, SunDirection.xyz, diffuseAlbedo.rgb, worldPos.rgb, roughness, F0, metalness, CameraPosition.xyz);

        for (uint i = 0; i < MAX_POINT_LIGHTS; i++)
        {
            PointLight light = PointLightsArray[i];
            if (light.PositionRadius.a <= 0.0f)
                continue;

            float3 dir = normalize(light.PositionRadius.rgb - worldPos.rgb);
            float attenuation = GetPointLightAttenuation(dir, light.PositionRadius.a);

            directLighting += DirectLightingPBR(normalWS, light.ColorIntensity * attenuation, dir, diffuseAlbedo.rgb, worldPos.rgb, roughness, F0, metalness, CameraPosition.xyz);
        }
    }
    
    if (useSSS)
    {
        directLighting += SunColor.rgb * diffuseAlbedo.rgb * 
            SSSSTransmittance(SSSTranslucency, SSSWidth, worldPos.rgb, normalWS, SunDirection.xyz, ShadowMatrices[0], CascadedPcfShadowMapSampler, ShadowTexelSize.x, CascadedShadowTextures[0], SSSDirectionLightMaxPlane);        
    }
    
    float3 indirectLighting = float3(0.0, 0.0, 0.0);
    if (isFoliage)
        indirectLighting = float3(0.1f, 0.1f, 0.1f) * diffuseAlbedo.rgb; // fake ambient for foliage
    
    bool skipIndirectLighting = (objectFlags & RENDERING_OBJECT_FLAG_SKIP_INDIRECT_DIF) && (objectFlags & RENDERING_OBJECT_FLAG_SKIP_INDIRECT_SPEC);

    if (!isFoliage && !skipIndirectLighting)
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
        
        indirectLighting += IndirectLightingPBR(SunDirection.xyz, normalWS, diffuseAlbedo.rgb, worldPos.rgb, roughness, F0, metalness, CameraPosition.xyz, useGlobalProbe > 0.0f,
            probesInfo, SamplerLinear, SamplerClamp, IntegrationTexture, ao, skipIndirectSpecular);
    }
    
    float shadow = Deferred_GetShadow(worldPos, ShadowMatrices, ShadowCascadeDistances, ShadowTexelSize.x, CascadedShadowTextures, CascadedPcfShadowMapSampler);
    
    float3 color = (directLighting * shadow) + indirectLighting;

    //debug shadow cascades
    if (ShadowCascadeDistances.w > 0.0)
    {
        float depthDistance = worldPos.a;
        if (depthDistance < ShadowCascadeDistances.x)
            color *= float4(1.0, 0.0, 0.0, 1.0);
        else if (depthDistance < ShadowCascadeDistances.y)
            color *= float4(0.0, 1.0, 0.0, 1.0);
        else if (depthDistance < ShadowCascadeDistances.z)
            color *= float4(0.0, 0.0, 1.0, 1.0);
        else
            color *= float4(1.0, 0.0, 1.0, 1.0);
    }

    OutputTexture[inPos] += float4(color, 1.0f);
}