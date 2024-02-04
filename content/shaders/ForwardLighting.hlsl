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
#include "IndirectCulling.hlsli"
#include "Common.hlsli"

Texture2D<float4> AlbedoTexture : register(t0);
Texture2D<float4> NormalTexture : register(t1);
Texture2D<float4> MetallicTexture : register(t2);
Texture2D<float4> RoughnessTexture : register(t3);
Texture2D<float> ExtraMaskTexture : register(t4); // transparent mask, POM height mask

// WARNING: Check Lighting.hlsli or Common.hlsli before binding into a specific index!

StructuredBuffer<Instance> IndirectInstanceData : register(t18);

cbuffer ForwardLightingCBuffer : register(b0)
{
    float4x4 ShadowMatrices[NUM_OF_SHADOW_CASCADES];
    float4x4 ViewProjection;
    float4 ShadowTexelSize;
    float4 ShadowCascadeDistances;
    float4 SunDirection;
    float4 SunColor;
    float4 CameraPosition;
}

// register(b1) is objects cbuffer from Common.hlsli

cbuffer LightProbesCBuffer : register(b2)
{
    float4 DiffuseProbesCellsCount; //x,y,z,total
    float4 SpecularProbesCellsCount; //x,y,z,total
    float4 SceneLightProbesBounds; //volume's extent of all scene's probes, w < 0.0 - 2D grid (i.e. terrain scene)
    float DistanceBetweenDiffuseProbes;
    float DistanceBetweenSpecularProbes;
}

cbuffer RootConstant : register(b3)
{
    uint CurrentLod;
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
    row_major float4x4 InstanceWorld : WORLD; // not used with indirect rendering
    uint InstanceID : SV_InstanceID;
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

    float4x4 WorldM = (RenderingObjectFlags & RENDERING_OBJECT_FLAG_GPU_INDIRECT_DRAW) ?
        transpose(IndirectInstanceData[(int)OriginalInstanceCount * CurrentLod + IN.InstanceID].WorldMat) : IN.InstanceWorld;

    OUT.WorldPos = mul(IN.Position, WorldM).xyz;
    OUT.Position = mul(float4(OUT.WorldPos, 1.0f), ViewProjection);
    OUT.UV = IN.Texcoord0;
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), WorldM).xyz);
    OUT.Tangent = normalize(mul(float4(IN.Tangent, 0), WorldM).xyz);
    OUT.ViewDir = IN.Position.xyz - CameraPosition.xyz;
    OUT.ShadowCoord0 = mul(IN.Position, mul(WorldM, ShadowMatrices[0])).xyz;
    OUT.ShadowCoord1 = mul(IN.Position, mul(WorldM, ShadowMatrices[1])).xyz;
    OUT.ShadowCoord2 = mul(IN.Position, mul(WorldM, ShadowMatrices[2])).xyz;
    
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
    float currentBound = ExtraMaskTexture.SampleGrad(SamplerLinear, texCurrentOffset, dx, dy).r;
    float softShadow = 0.0f;
    
    while (stepIndex < numSteps)
    {
        texCurrentOffset += texOffsetPerStep;
        currentHeight = ExtraMaskTexture.SampleGrad(SamplerLinear, texCurrentOffset, dx, dy).r;

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
        currentHeight = ExtraMaskTexture.SampleGrad(SamplerLinear, texCurrentOffset, dx, dy).r;

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

float3 GetFinalColor(VS_OUTPUT vsOutput, bool IBL, int forcedCascadeShadowIndex = -1, bool isFakeAmbient = false, bool isTransparent = false)
{
    float3x3 TBN = float3x3(vsOutput.Tangent, cross(vsOutput.Normal, vsOutput.Tangent), vsOutput.Normal);
    float2 texCoord = vsOutput.UV;
    
    float3 normalWS = float3(0.0, 0.0, 0.0);

    bool useTriplanarMapping = (RenderingObjectFlags & RENDERING_OBJECT_FLAG_TRIPLANAR_MAPPING) && !(RenderingObjectFlags & RENDERING_OBJECT_FLAG_POM);

    float extraMaskValue = -1.0f; 
    if ((RenderingObjectFlags & RENDERING_OBJECT_FLAG_POM) || (RenderingObjectFlags & RENDERING_OBJECT_FLAG_TRANSPARENT))
    {
        if (useTriplanarMapping)
            extraMaskValue = GetTriplanarMapping(ExtraMaskTexture, SamplerLinear, vsOutput.WorldPos.xyz, vsOutput.Normal.xyz, TRIPLANAR_MAPPING_BLEND_SHARPNESS);
        else
            extraMaskValue = ExtraMaskTexture.Sample(SamplerLinear, texCoord).r;
    }
    
    float POMSelfShadow = 1.0f;
#if PARALLAX_OCCLUSION_MAPPING_SUPPORT
    if (!isTransparent && (RenderingObjectFlags & RENDERING_OBJECT_FLAG_POM) && extraMaskValue >= 0.0f) // Parallax Occlusion Mapping
    {
        int stepsCount = GetPOMRayStepsCount(vsOutput.WorldPos.rgb, vsOutput.Normal);
        texCoord = CalculatePOMUVOffset(vsOutput.ParallaxOffset, vsOutput.UV, stepsCount);
        float3 sampledNormal = float3(0.0, 0.0, 0.0);
        if (!useTriplanarMapping)
            sampledNormal = 2.0 * NormalTexture.Sample(SamplerLinear, texCoord).xyz - 1.0;
        else
            sampledNormal = 2.0 * GetTriplanarMapping(NormalTexture, SamplerLinear, vsOutput.WorldPos.xyz, vsOutput.Normal.xyz, TRIPLANAR_MAPPING_BLEND_SHARPNESS).xyz - 1.0;
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
        float3 sampledNormal = float3(0.0, 0.0, 0.0);
        if (!useTriplanarMapping)
            sampledNormal = 2.0 * NormalTexture.Sample(SamplerLinear, texCoord).xyz - 1.0;
        else
            sampledNormal = 2.0 * GetTriplanarMapping(NormalTexture, SamplerLinear, vsOutput.WorldPos.xyz, vsOutput.Normal.xyz, TRIPLANAR_MAPPING_BLEND_SHARPNESS).xyz - 1.0;
        sampledNormal = mul(sampledNormal, TBN);
        normalWS = normalize(sampledNormal);
    }
    
    float4 diffuseAlbedoNonGamma = float4(0.0, 0.0, 0.0, 0.0);
    if (!useTriplanarMapping)
        diffuseAlbedoNonGamma = AlbedoTexture.Sample(SamplerLinear, texCoord);
    else
        diffuseAlbedoNonGamma = GetTriplanarMapping(AlbedoTexture, SamplerLinear, vsOutput.WorldPos.xyz, vsOutput.Normal.xyz, TRIPLANAR_MAPPING_BLEND_SHARPNESS);

    clip(diffuseAlbedoNonGamma.a < 0.000001f ? -1 : 1);
    float3 diffuseAlbedo = pow(diffuseAlbedoNonGamma.rgb, 2.2);
    
    float metalness = 0.0f;
    if (CustomMetalness >= 0.0f)
        metalness = CustomMetalness;
    else
    {
        if (!useTriplanarMapping)
            metalness = MetallicTexture.Sample(SamplerLinear, texCoord);
        else
            metalness = GetTriplanarMapping(MetallicTexture, SamplerLinear, vsOutput.WorldPos.xyz, vsOutput.Normal.xyz, TRIPLANAR_MAPPING_BLEND_SHARPNESS);
    }

    float roughness = 0.0f;
    if (CustomRoughness >= 0.0f)
        roughness = CustomRoughness;
    else
    {
        if (!useTriplanarMapping)
            roughness = RoughnessTexture.Sample(SamplerLinear, texCoord);
        else
            roughness = GetTriplanarMapping(RoughnessTexture, SamplerLinear, vsOutput.WorldPos.xyz, vsOutput.Normal.xyz, TRIPLANAR_MAPPING_BLEND_SHARPNESS);
    }

    float ao = 1.0f; // TODO sample AO texture
    
    //reflectance at normal incidence for dia-electic or metal
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, diffuseAlbedo.rgb, metalness);

    if (isTransparent)
    {
        float transparencyMask = extraMaskValue; //just a hack to get transparency mask texture in height channel (we dont need it here anyway)
        float3 viewDir = normalize(vsOutput.WorldPos.xyz-CameraPosition.xyz);
        float3 reflectDir = normalize(reflect(viewDir, normalWS));
        float3 T = refract(viewDir, normalWS, 1.0f / IndexOfRefraction);

        int mipIndex = roughness * (SPECULAR_PROBE_MIP_COUNT - 1);

        // get closest specular light probe (unless specified to use global)
        LightProbeInfo info;
        info.globalIrradianceDiffuseProbeTexture = DiffuseGlobalProbeTexture;
        info.globalIrradianceSpecularProbeTexture = SpecularGlobalProbeTexture;
        info.DiffuseProbesCellsWithProbeIndicesArray = DiffuseProbesCellsWithProbeIndicesArray;
        info.DiffuseSphericalHarmonicsCoefficientsArray = DiffuseSphericalHarmonicsCoefficientsArray;
        info.DiffuseProbesPositionsArray = DiffuseProbesPositionsArray;
        info.SpecularProbesTextureArray = SpecularProbesTextureArray;
        info.SpecularProbesCellsWithProbeIndicesArray = SpecularProbesCellsWithProbeIndicesArray;
        info.SpecularProbesTextureArrayIndices = SpecularProbesTextureArrayIndices;
        info.SpecularProbesPositionsArray = SpecularProbesPositionsArray;
        info.diffuseProbeCellsCount = DiffuseProbesCellsCount;
        info.specularProbeCellsCount = SpecularProbesCellsCount;
        info.sceneLightProbeBounds = SceneLightProbesBounds;
        info.distanceBetweenDiffuseProbes = DistanceBetweenDiffuseProbes;
        info.distanceBetweenSpecularProbes = DistanceBetweenSpecularProbes;

        bool useGlobalSpecProbe = (RenderingObjectFlags & RENDERING_OBJECT_FLAG_USE_GLOBAL_SPEC_PROBE);
        float3 reflectColor = pow(GetSpecularIrradiance(vsOutput.WorldPos.xyz, CameraPosition.xyz, reflectDir, mipIndex, useGlobalSpecProbe, SamplerLinear, info), 2.2);
        float3 refractColor = pow(GetSpecularIrradiance(vsOutput.WorldPos.xyz, CameraPosition.xyz, T, mipIndex, useGlobalSpecProbe, SamplerLinear, info), 2.2);
        
        float nDotV = abs(dot(normalWS, -viewDir)) + 0.0001f;
        float3 fresnelFactor = Schlick_Fresnel_Roughness(nDotV, F0, roughness);
        float3 resultColor = lerp(refractColor, reflectColor, fresnelFactor);

        return lerp(diffuseAlbedo, diffuseAlbedo * resultColor, transparencyMask);
    }

    float3 directLighting = DirectLightingPBR(normalWS, SunColor, SunDirection.xyz, diffuseAlbedo.rgb, vsOutput.WorldPos, roughness, F0, metalness, CameraPosition.xyz);
    
	float3 pointLighting = float3(0.0, 0.0, 0.0);
    for (uint i = 0; i < MAX_POINT_LIGHTS; i++)
    {
        PointLight light = PointLightsArray[i];
        if (light.PositionRadius.a <= 0.0f)
            continue;

        float3 lightVec = float3(light.PositionRadius.rgb - vsOutput.WorldPos);
        float3 dir = normalize(lightVec);
        
        //float distanceSqr = dot(lightVec, lightVec);
        float distance = length(lightVec);
        float attenuation = GetPointLightAttenuation(distance, light.PositionRadius.a); //(1 / (distanceSqr + 1));

        pointLighting += DirectLightingPBR(normalWS,  light.ColorIntensity * attenuation , dir, diffuseAlbedo.rgb, vsOutput.WorldPos, roughness, F0, metalness, CameraPosition.xyz);
    }    

    float3 indirectLighting = float3(0, 0, 0);
    if (isFakeAmbient)
        indirectLighting = float3(0.02f, 0.02f, 0.02f) * diffuseAlbedo * SunColor.rgb * SunColor.a;
    
    bool skipIndirectLighting = (RenderingObjectFlags & RENDERING_OBJECT_FLAG_SKIP_INDIRECT_DIF) && (RenderingObjectFlags & RENDERING_OBJECT_FLAG_SKIP_INDIRECT_SPEC);
    if (IBL && !skipIndirectLighting)
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
        
        bool useGlobalProbe = (RenderingObjectFlags & RENDERING_OBJECT_FLAG_USE_GLOBAL_DIF_PROBE) || (RenderingObjectFlags & RENDERING_OBJECT_FLAG_USE_GLOBAL_SPEC_PROBE);
        indirectLighting += IndirectLightingPBR(SunDirection.xyz, normalWS, diffuseAlbedo.rgb, vsOutput.WorldPos, roughness, F0, metalness, CameraPosition.xyz,
            useGlobalProbe, probesInfo, SamplerLinear, SamplerClamp, IntegrationTexture, ao, false);
    }

    float shadow = 0.0f;
    float3 shadowCoords[3] = { vsOutput.ShadowCoord0, vsOutput.ShadowCoord1, vsOutput.ShadowCoord2 };
    if (forcedCascadeShadowIndex == -2) // disabled shadows
        shadow = 1.0f;
    else // standard 3 cascades or forced cascade
        shadow = Forward_GetShadow(ShadowCascadeDistances, shadowCoords, ShadowTexelSize.r, CascadedShadowTextures, CascadedPcfShadowMapSampler, vsOutput.Position.w, forcedCascadeShadowIndex);

    float3 color = ((directLighting * shadow + pointLighting) * POMSelfShadow) + indirectLighting;
    return color;
}

float3 PSMain(VS_OUTPUT vsOutput) : SV_Target0
{
    return GetFinalColor(vsOutput, true);
}
float4 PSMain_Transparent(VS_OUTPUT vsOutput) : SV_Target0
{
    return float4(GetFinalColor(vsOutput, true, -1, false, true), 1.0);
}
float3 PSMain_DiffuseProbes(VS_OUTPUT vsOutput) : SV_Target0
{
    //since we dont do tonemapping for probe rendering, run gamma correction
    return GetGammaCorrectColor(GetFinalColor(vsOutput, false, NUM_OF_SHADOW_CASCADES - 1, true));
}
float3 PSMain_SpecularProbes(VS_OUTPUT vsOutput) : SV_Target0
{
    //since we dont do tonemapping for probe rendering, run gamma correction
    return GetGammaCorrectColor(GetFinalColor(vsOutput, false, NUM_OF_SHADOW_CASCADES - 1, true));
}