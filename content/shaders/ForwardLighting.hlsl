// ================================================================================================
// Vertex/Pixel shader for classic "Forward" lighting. 
//
// Supports:
// - Cascaded Shadow Mapping
// - PBR with Image Based Lighting (via light probes)
// - Parallax-Occlusion Mapping
// - Instancing
//
// TODO:
// - move to "Forward+"
// - add support for proper transparency (+BRDF)
// - add support for point/spot lights
// - add support for ambient occlusion
//
// Info: also used for rendering into light probes cubemaps (with different entry points for PS)
//
// Written by Gen Afanasev for 'EveryRay Rendering Engine', 2017-2022
// ================================================================================================

#include "Lighting.hlsli"
#include "Common.hlsli"

Texture2D<float4> AlbedoTexture : register(t0);
Texture2D<float4> NormalTexture : register(t1);
Texture2D<float4> MetallicTexture : register(t2);
Texture2D<float4> RoughnessTexture : register(t3);
Texture2D<float4> HeightTexture : register(t4);
Texture2D<float> CascadedShadowTextures[NUM_OF_SHADOW_CASCADES] : register(t5);

SamplerState SamplerLinear : register(s0);
SamplerComparisonState CascadedPcfShadowMapSampler : register(s1);

cbuffer ForwardLightingCBuffer : register(b0)
{
    float4x4 ShadowMatrices[NUM_OF_SHADOW_CASCADES];
    float4x4 ViewProjection;
    float4x4 World;
    float4 ShadowTexelSize;
    float4 ShadowCascadeDistances;
    float4 SunDirection;
    float4 SunColor;
    float4 CameraPosition;
    float UseGlobalProbe;
    float SkipIndirectProbeLighting;
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

struct VS_INPUT
{
    float4 Position : POSITION;
    float2 Texcoord0 : TEXCOORD;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
};

struct VS_INPUT_INSTANCING
{
    float4 Position : POSITION;
    float2 Texcoord0 : TEXCOORD;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    
    //instancing
    row_major float4x4 World : WORLD;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float3 WorldPos : WorldPos;
    float2 UV : TexCoord0;
    float3 ViewDir : TexCoord1;
    float3 ShadowCoord0 : TexCoord2;
    float3 ShadowCoord1 : TexCoord3;
    float3 ShadowCoord2 : TexCoord4;
    float3 Normal : Normal;
    float3 Tangent : Tangent;
#if PARALLAX_OCCLUSION_MAPPING_SUPPORT
    float2 ParallaxOffset : ParallaxOffset;
    float2 ParallaxSelfShadowOffset : ParallaxSelfShadowOffset;
#endif
};


VS_OUTPUT VSMain(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;

    OUT.Position = mul(IN.Position, mul(World, ViewProjection));
    OUT.WorldPos = mul(IN.Position, World).xyz;
    OUT.UV = IN.Texcoord0;
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), World).xyz);
    OUT.Tangent = normalize(mul(float4(IN.Tangent, 0), World).xyz);
    OUT.ViewDir = IN.Position.xyz - CameraPosition.xyz;
    OUT.ShadowCoord0 = mul(IN.Position, mul(World, ShadowMatrices[0])).xyz;
    OUT.ShadowCoord1 = mul(IN.Position, mul(World, ShadowMatrices[1])).xyz;
    OUT.ShadowCoord2 = mul(IN.Position, mul(World, ShadowMatrices[2])).xyz;
    
#if PARALLAX_OCCLUSION_MAPPING_SUPPORT
    float3x3 TBN = float3x3(OUT.Tangent, cross(OUT.Normal, OUT.Tangent), OUT.Normal);
    float3 TangentViewDir = CameraPosition.rgb - OUT.WorldPos.rgb;
    TangentViewDir = mul(TBN, TangentViewDir);

    // calculating parallax offsets in vertex shader as optimization
    float2 parallaxDir = normalize(TangentViewDir.xy);
    float viewDirTSLength = length(TangentViewDir);
    float parallaxLength = sqrt(viewDirTSLength * viewDirTSLength - TangentViewDir.z * TangentViewDir.z) / TangentViewDir.z;
    OUT.ParallaxOffset = parallaxDir * parallaxLength * PARALLAX_OCCLUSION_MAPPING_HEIGHT_SCALE;
    
#if PARALLAX_OCCLUSION_MAPPING_SELF_SHADOW_SUPPORT
    float3 TangentLightDir = mul(TBN, SunDirection.xyz);
    
    float2 parallaxLightDir = normalize(TangentLightDir.xy);
    float parallaxLightDirTSLength = length(TangentLightDir);
    float parallaxLightDirLength = sqrt(parallaxLightDirTSLength * parallaxLightDirTSLength - TangentLightDir.z * TangentLightDir.z) / TangentLightDir.z;
    OUT.ParallaxSelfShadowOffset = parallaxLightDir * parallaxLightDirLength * PARALLAX_OCCLUSION_MAPPING_HEIGHT_SCALE;
#endif
    
#endif
    return OUT;
}

VS_OUTPUT VSMain_instancing(VS_INPUT_INSTANCING IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;

    OUT.WorldPos = mul(IN.Position, IN.World).xyz;
    OUT.Position = mul(float4(OUT.WorldPos, 1.0f), ViewProjection);
    OUT.UV = IN.Texcoord0;
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), IN.World).xyz);
    OUT.Tangent = normalize(mul(float4(IN.Tangent, 0), IN.World).xyz);
    OUT.ViewDir = IN.Position.xyz - CameraPosition.xyz;
    OUT.ShadowCoord0 = mul(IN.Position, mul(IN.World, ShadowMatrices[0])).xyz;
    OUT.ShadowCoord1 = mul(IN.Position, mul(IN.World, ShadowMatrices[1])).xyz;
    OUT.ShadowCoord2 = mul(IN.Position, mul(IN.World, ShadowMatrices[2])).xyz;
    
#if PARALLAX_OCCLUSION_MAPPING_SUPPORT
    float3x3 TBN = float3x3(OUT.Tangent, cross(OUT.Normal, OUT.Tangent), OUT.Normal);
    float3 TangentViewDir = CameraPosition.rgb - OUT.WorldPos.rgb;
    TangentViewDir = mul(TBN, TangentViewDir);

    // calculating parallax offsets in vertex shader as optimization
    float2 parallaxDir = normalize(TangentViewDir.xy);
    float viewDirTSLength = length(TangentViewDir);
    float parallaxLength = sqrt(viewDirTSLength * viewDirTSLength - TangentViewDir.z * TangentViewDir.z) / TangentViewDir.z;
    OUT.ParallaxOffset = parallaxDir * parallaxLength * PARALLAX_OCCLUSION_MAPPING_HEIGHT_SCALE;
    
#if PARALLAX_OCCLUSION_MAPPING_SELF_SHADOW_SUPPORT
    float3 TangentLightDir = mul(TBN, SunDirection.xyz);
    
    float2 parallaxLightDir = normalize(TangentLightDir.xy);
    float parallaxLightDirTSLength = length(TangentLightDir);
    float parallaxLightDirLength = sqrt(parallaxLightDirTSLength * parallaxLightDirTSLength - TangentLightDir.z * TangentLightDir.z) / TangentLightDir.z;
    OUT.ParallaxSelfShadowOffset = parallaxLightDir * parallaxLightDirLength * PARALLAX_OCCLUSION_MAPPING_HEIGHT_SCALE;
#endif
    
#endif
    return OUT;
}

// ================================================================================================
// Parallax-Occlusion Mapping (with soft self-shadowing)
// ================================================================================================
float CalculatePOMSelfShadow(float2 parallaxOffset, float2 uv, int numSteps)
{
#if PARALLAX_OCCLUSION_MAPPING_SUPPORT
    float2 dx = ddx(uv);
    float2 dy = ddy(uv);
          
    float currentHeight = 0.0f;
    float stepSize = 1.0f / (float) numSteps;
    int stepIndex = 0;

    float2 texOffsetPerStep = stepSize * parallaxOffset;
    float2 texCurrentOffset = uv;
    float currentBound = HeightTexture.SampleGrad(SamplerLinear, texCurrentOffset, dx, dy).r;
    float softShadow = 0.0f;
    
    while (stepIndex < numSteps)
    {
        texCurrentOffset += texOffsetPerStep;
        currentHeight = HeightTexture.SampleGrad(SamplerLinear, texCurrentOffset, dx, dy).r;

        currentBound += stepSize;

        if (currentHeight > currentBound)
        {
            float newSoftShadow = (currentHeight - currentBound) * (1.0 - (float) stepIndex / (float) numSteps);
            softShadow = max(softShadow, newSoftShadow);
        }
        else
            stepIndex++;
    }
    
    float shadow = (stepIndex >= numSteps) ? (1.0f - clamp(PARALLAX_OCCLUSION_MAPPING_SOFT_SHADOWS_FACTOR * softShadow, 0.0f, 1.0f)) : 1.0f;
    return shadow;
#else
    return 0.0f;
#endif
}
float2 CalculatePOMUVOffset(float2 parallaxOffset, float2 uv, int numSteps)
{
#if PARALLAX_OCCLUSION_MAPPING_SUPPORT
    float currentHeight = 0.0f;
    float stepSize = 1.0f / (float) numSteps;
    float prevHeight = 1.0f;
    float nextHeight = 0.0f;
    int stepIndex = 0;

    float2 texOffsetPerStep = stepSize * parallaxOffset;
    float2 texCurrentOffset = uv;
    float currentBound = 1.0;
    float parallaxAmount = 0.0;

    float2 pt1 = 0;
    float2 pt2 = 0;

    float2 texOffset2 = 0;
    float2 dx = ddx(uv);
    float2 dy = ddy(uv);
    
    while (stepIndex < numSteps)
    {
        texCurrentOffset -= texOffsetPerStep;
        currentHeight = HeightTexture.SampleGrad(SamplerLinear, texCurrentOffset, dx, dy).r;

        currentBound -= stepSize;

        if (currentHeight > currentBound)
        {
            pt1 = float2(currentBound, currentHeight); // point from current height
            pt2 = float2(currentBound + stepSize, prevHeight); // point from previous height

            texOffset2 = texCurrentOffset - texOffsetPerStep;

            stepIndex = numSteps + 1;
        }
        else
        {
            stepIndex++;
            prevHeight = currentHeight;
        }
    }
   
    //linear interpolation
    float delta2 = pt2.x - pt2.y;
    float delta1 = pt1.x - pt1.y;
    float diff = delta2 - delta1;
      
    if (diff == 0.0f)
        parallaxAmount = 0.0f;
    else
        parallaxAmount = (pt1.x * delta2 - pt2.x * delta1) / diff;
   
    float2 vParallaxOffset = parallaxOffset * (1.0 - parallaxAmount);
    return uv - vParallaxOffset;
#else
    return uv;
#endif
}
int GetPOMRayStepsCount(float3 worldPos, float3 normal)
{
    int minLayers = 8;
    int maxLayers = 32;
    
    int numLayers = (int) lerp(maxLayers, minLayers, dot(normalize(CameraPosition.rgb - worldPos), normal));
    return numLayers;
}

float3 GetFinalColor(VS_OUTPUT vsOutput, bool IBL, int forcedCascadeShadowIndex = -1)
{
    float3x3 TBN = float3x3(vsOutput.Tangent, cross(vsOutput.Normal, vsOutput.Tangent), vsOutput.Normal);
    float2 texCoord = vsOutput.UV;
    
    float3 normalWS = float3(0.0, 0.0, 0.0);
    float height = HeightTexture.Sample(SamplerLinear, texCoord).r;
    float POMSelfShadow = 1.0f;
#if PARALLAX_OCCLUSION_MAPPING_SUPPORT
    if (height > 0.0f) // Parallax Occlusion Mapping
    {
        int stepsCount = GetPOMRayStepsCount(vsOutput.WorldPos.rgb, vsOutput.Normal);
        texCoord = CalculatePOMUVOffset(vsOutput.ParallaxOffset, vsOutput.UV, stepsCount);
        float3 sampledNormal = (2 * NormalTexture.Sample(SamplerLinear, texCoord).xyz) - 1.0;
        sampledNormal = mul(sampledNormal, TBN);
        normalWS = normalize(sampledNormal);
        
#if PARALLAX_OCCLUSION_MAPPING_SELF_SHADOW_SUPPORT
        if (max(dot(normalWS, SunDirection.xyz), 0.0f) > 0.0f)
            POMSelfShadow = CalculatePOMSelfShadow(vsOutput.ParallaxSelfShadowOffset, texCoord, stepsCount);
#endif
    }
    else
#endif
    {
        float3 sampledNormal = (2 * NormalTexture.Sample(SamplerLinear, texCoord).xyz) - 1.0;
        sampledNormal = mul(sampledNormal, TBN);
        normalWS = normalize(sampledNormal);
    }
    
    float4 diffuseAlbedoNonGamma = AlbedoTexture.Sample(SamplerLinear, texCoord);
    clip(diffuseAlbedoNonGamma.a < 0.000001f ? -1 : 1);
    float3 diffuseAlbedo = pow(diffuseAlbedoNonGamma.rgb, 2.2);
    
    float metalness = MetallicTexture.Sample(SamplerLinear, texCoord).r;
    float roughness = RoughnessTexture.Sample(SamplerLinear, texCoord).r;
    float ao = 1.0f; // TODO sample AO texture
    
    //reflectance at normal incidence for dia-electic or metal
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, diffuseAlbedo.rgb, metalness);

    float3 directLighting = DirectLightingPBR(normalWS, SunColor, SunDirection.xyz, diffuseAlbedo.rgb, vsOutput.WorldPos, roughness, F0, metalness, CameraPosition.xyz);
    
    float3 indirectLighting = float3(0, 0, 0);
    if (IBL && SkipIndirectProbeLighting <= 0.0f)
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
        
        bool useGlobalProbe = UseGlobalProbe > 0.0f;
        indirectLighting += IndirectLightingPBR(normalWS, diffuseAlbedo.rgb, vsOutput.WorldPos, roughness, F0, metalness, CameraPosition.xyz, useGlobalProbe,
            probesInfo, SamplerLinear, IntegrationTexture, ao);
    }

    float shadow = 0.0f;
    float3 shadowCoords[3] = { vsOutput.ShadowCoord0, vsOutput.ShadowCoord1, vsOutput.ShadowCoord2 };
    if (forcedCascadeShadowIndex == -2) // disabled shadows
        shadow = 1.0f;
    else // standard 3 cascades or forced cascade
        shadow = Forward_GetShadow(ShadowCascadeDistances, shadowCoords, ShadowTexelSize.r, CascadedShadowTextures, CascadedPcfShadowMapSampler, vsOutput.Position.w, forcedCascadeShadowIndex);
    
    float3 color = (directLighting * shadow * POMSelfShadow) + indirectLighting;
    return color;
}

float3 PSMain(VS_OUTPUT vsOutput) : SV_Target0
{
    return GetFinalColor(vsOutput, true);
}
float3 PSMain_DiffuseProbes(VS_OUTPUT vsOutput) : SV_Target0
{
    //since we dont do tonemapping for probe rendering, run gamma correction
    return GetGammaCorrectColor(GetFinalColor(vsOutput, false, -2));
}
float3 PSMain_SpecularProbes(VS_OUTPUT vsOutput) : SV_Target0
{
    //since we dont do tonemapping for probe rendering, run gamma correction
    return GetGammaCorrectColor(GetFinalColor(vsOutput, false, NUM_OF_SHADOW_CASCADES - 1));
}