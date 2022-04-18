#define FLT_MAX 3.402823466e+38

#define NUM_OF_SHADOW_CASCADES 3
#define NUM_OF_PROBES_PER_CELL 8
#define NUM_OF_PROBE_VOLUME_CASCADES 2
#define SPECULAR_PROBE_MIP_COUNT 6

#define PARALLAX_OCCLUSION_MAPPING_SUPPORT 1
#define PARALLAX_OCCLUSION_MAPPING_SELF_SHADOW_SUPPORT 1
#define PARALLAX_OCCLUSION_MAPPING_HEIGHT_SCALE 0.05
#define PARALLAX_OCCLUSION_MAPPING_SOFT_SHADOWS_FACTOR 10.0

static const float4 ColorWhite = { 1, 1, 1, 1 };
static const float Pi = 3.141592654f;

//texture arrays of cubemap probes (unfortunately, without bindless support there is no way to have array of arrays)
//TODO consider Spherical-Harmonics for diffuse probes (will get rid of cubemaps)
TextureCubeArray<float4> IrradianceDiffuseProbesTextureArray0 : register(t8); //cascade 0
TextureCubeArray<float4> IrradianceDiffuseProbesTextureArray1 : register(t9); //cascade 1
TextureCube<float4> IrradianceDiffuseGlobalProbeTexture : register(t10); // global probe (fallback)

TextureCubeArray<float4> IrradianceSpecularProbesTextureArray0 : register(t11); //cascade 0
TextureCubeArray<float4> IrradianceSpecularProbesTextureArray1 : register(t12); //cascade 1
TextureCube<float4> IrradianceSpecularGlobalProbeTexture : register(t13); // global probe (fallback)

Texture2D<float4> IntegrationTexture : register(t14);

StructuredBuffer<int> DiffuseProbesCellsWithProbeIndices0 : register(t15); //cascade 0, linear array of cells with NUM_OF_PROBES_PER_CELL probes' indices in each cell
StructuredBuffer<int> DiffuseProbesCellsWithProbeIndices1 : register(t16); //cascade 1, linear array of cells with NUM_OF_PROBES_PER_CELL probes' indices in each cell
StructuredBuffer<int> DiffuseProbesTextureArrayIndices0 : register(t17); //cascade 0, array of all diffuse probes in scene with indices in 'IrradianceDiffuseProbesTextureArray' for each probe (-1 if not in array)
StructuredBuffer<int> DiffuseProbesTextureArrayIndices1 : register(t18); //cascade 1, array of all diffuse probes in scene with indices in 'IrradianceDiffuseProbesTextureArray' for each probe (-1 if not in array)
StructuredBuffer<float3> DiffuseProbesPositions : register(t19); //linear array of all diffuse probes positions

StructuredBuffer<int> SpecularProbesCellsWithProbeIndices0 : register(t20); //cascade 0, linear array of cells with NUM_OF_PROBES_PER_CELL probes' indices in each cell
StructuredBuffer<int> SpecularProbesCellsWithProbeIndices1 : register(t21); //cascade 1, linear array of cells with NUM_OF_PROBES_PER_CELL probes' indices in each cell
StructuredBuffer<int> SpecularProbesTextureArrayIndices0 : register(t22); //cascade 0, array of all specular probes in scene with indices in 'IrradianceSpecularProbesTextureArray' for each probe (-1 if not in array)
StructuredBuffer<int> SpecularProbesTextureArrayIndices1 : register(t23); //cascade 1, array of all specular probes in scene with indices in 'IrradianceSpecularProbesTextureArray' for each probe (-1 if not in array)
StructuredBuffer<float3> SpecularProbesPositions : register(t24); //linear array of all specular probes positions

struct LightProbeInfo
{
    TextureCube<float4> globalIrradianceDiffuseProbeTexture;
    TextureCube<float4> globalIrradianceSpecularProbeTexture;
    TextureCubeArray<float4> IrradianceDiffuseProbesTextureArray0;
    TextureCubeArray<float4> IrradianceDiffuseProbesTextureArray1;
    TextureCubeArray<float4> IrradianceSpecularProbesTextureArray0;
    TextureCubeArray<float4> IrradianceSpecularProbesTextureArray1;
    
    StructuredBuffer<int> DiffuseProbesCellsWithProbeIndices0;
    StructuredBuffer<int> DiffuseProbesCellsWithProbeIndices1;
    StructuredBuffer<int> DiffuseProbesTextureArrayIndices0;
    StructuredBuffer<int> DiffuseProbesTextureArrayIndices1;
    StructuredBuffer<float3> DiffuseProbesPositions;
    StructuredBuffer<int> SpecularProbesCellsWithProbeIndices0;
    StructuredBuffer<int> SpecularProbesCellsWithProbeIndices1;
    StructuredBuffer<int> SpecularProbesTextureArrayIndices0;
    StructuredBuffer<int> SpecularProbesTextureArrayIndices1;
    StructuredBuffer<float3> SpecularProbesPositions;
    
    float4 diffuseProbesVolumeSizes[NUM_OF_PROBE_VOLUME_CASCADES];
    float4 diffuseProbeCellsCount[NUM_OF_PROBE_VOLUME_CASCADES];
    float4 specularProbesVolumeSizes[NUM_OF_PROBE_VOLUME_CASCADES];
    float4 specularProbeCellsCount[NUM_OF_PROBE_VOLUME_CASCADES];
    float4 volumeProbesIndexSkips[NUM_OF_PROBE_VOLUME_CASCADES];
    float4 sceneLightProbeBounds;
    float distanceBetweenDiffuseProbes;
    float distanceBetweenSpecularProbes;
};

// ================================================================================================
// Simple PCF cascaded shadow mapping (deferred & forward versions)
// ================================================================================================
float Deferred_CalculateCSM(float3 worldPos, float4x4 svp, int index, float ShadowTexelSize, in Texture2D<float> CascadedShadowTexture, in SamplerComparisonState CascadedPcfShadowMapSampler)
{
    float4 lightSpacePos = mul(svp, float4(worldPos, 1.0f));
    float4 ShadowCoord = lightSpacePos / lightSpacePos.w;
    ShadowCoord.rg = ShadowCoord.rg * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    
    const float Dilation = 2.0;
    float d1 = Dilation * ShadowTexelSize * 0.125;
    float d2 = Dilation * ShadowTexelSize * 0.875;
    float d3 = Dilation * ShadowTexelSize * 0.625;
    float d4 = Dilation * ShadowTexelSize * 0.375;
    float result = (
        2.0 * CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy, ShadowCoord.z) +
        CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d2, d1), ShadowCoord.z) +
        CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d1, -d2), ShadowCoord.z) +
        CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d2, -d1), ShadowCoord.z) +
        CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d1, d2), ShadowCoord.z) +
        CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d4, d3), ShadowCoord.z) +
        CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d3, -d4), ShadowCoord.z) +
        CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d4, -d3), ShadowCoord.z) +
        CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d3, d4), ShadowCoord.z)
        ) / 10.0;

    return result * result;
}
float Deferred_GetShadow(float4 worldPos, float4x4 ShadowMatrices[NUM_OF_SHADOW_CASCADES], float4 ShadowCascadeDistances, float ShadowTexelSize, in Texture2D<float> CascadedShadowTextures[NUM_OF_SHADOW_CASCADES],
    in SamplerComparisonState CascadedPcfShadowMapSampler)
{
    float depthDistance = worldPos.a;
    
    if (depthDistance < ShadowCascadeDistances.x)
        return Deferred_CalculateCSM(worldPos.rgb, ShadowMatrices[0], 0, ShadowTexelSize, CascadedShadowTextures[0], CascadedPcfShadowMapSampler);
    else if (depthDistance < ShadowCascadeDistances.y)
        return Deferred_CalculateCSM(worldPos.rgb, ShadowMatrices[1], 1, ShadowTexelSize, CascadedShadowTextures[1], CascadedPcfShadowMapSampler);
    else if (depthDistance < ShadowCascadeDistances.z)
        return Deferred_CalculateCSM(worldPos.rgb, ShadowMatrices[2], 2, ShadowTexelSize, CascadedShadowTextures[2], CascadedPcfShadowMapSampler);
    else
        return 1.0f;
}
float Forward_CalculateCSM(float3 ShadowCoord, float ShadowTexelSize, in Texture2D<float> CascadedShadowTexture, in SamplerComparisonState CascadedPcfShadowMapSampler)
{
    const float Dilation = 2.0;
    float d1 = Dilation * ShadowTexelSize * 0.125;
    float d2 = Dilation * ShadowTexelSize * 0.875;
    float d3 = Dilation * ShadowTexelSize * 0.625;
    float d4 = Dilation * ShadowTexelSize * 0.375;
    float result = (
        2.0 * CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy, ShadowCoord.z) +
        CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d2, d1), ShadowCoord.z) +
        CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d1, -d2), ShadowCoord.z) +
        CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d2, -d1), ShadowCoord.z) +
        CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d1, d2), ShadowCoord.z) +
        CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d4, d3), ShadowCoord.z) +
        CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d3, -d4), ShadowCoord.z) +
        CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d4, -d3), ShadowCoord.z) +
        CascadedShadowTexture.SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d3, d4), ShadowCoord.z)
        ) / 10.0;

    return result * result;
}
float Forward_GetShadow(float4 ShadowCascadeDistances, float3 ShadowCoords[NUM_OF_SHADOW_CASCADES], float ShadowTexelSize, in Texture2D<float> CascadedShadowTextures[NUM_OF_SHADOW_CASCADES],
    in SamplerComparisonState CascadedPcfShadowMapSampler, float depthDistance, int nthCascade = -1)
{
    if (nthCascade == -1)
    {
        if (depthDistance < ShadowCascadeDistances.x)
            return Forward_CalculateCSM(ShadowCoords[0], ShadowTexelSize, CascadedShadowTextures[0], CascadedPcfShadowMapSampler);
        else if (depthDistance < ShadowCascadeDistances.y)
            return Forward_CalculateCSM(ShadowCoords[1], ShadowTexelSize, CascadedShadowTextures[1], CascadedPcfShadowMapSampler);
        else if (depthDistance < ShadowCascadeDistances.z)
            return Forward_CalculateCSM(ShadowCoords[2], ShadowTexelSize, CascadedShadowTextures[2], CascadedPcfShadowMapSampler);
        else
            return 1.0f;
    }
    
    // Besides standard CSM, we can force render a specific cascade (i.e., thats useful for light probe rendering which happens in forward mode)
    if (nthCascade == 0)
    {
        if (depthDistance < ShadowCascadeDistances.x)
            return Forward_CalculateCSM(ShadowCoords[0], ShadowTexelSize, CascadedShadowTextures[0], CascadedPcfShadowMapSampler);
        else
            return 1.0f;
    }
    else if (nthCascade == 1)
    {
        if (depthDistance < ShadowCascadeDistances.y)
            return Forward_CalculateCSM(ShadowCoords[1], ShadowTexelSize, CascadedShadowTextures[1], CascadedPcfShadowMapSampler);
        return
            1.0f;
    }
    else
    {
        if (depthDistance < ShadowCascadeDistances.z)
            return Forward_CalculateCSM(ShadowCoords[2], ShadowTexelSize, CascadedShadowTextures[2], CascadedPcfShadowMapSampler);
        return
            1.0f;
    }
}

// ===============================================================================================
// GGX functions
// http://graphicrants.blogspot.com.au/2013/08/specular-brdf-reference.html
// ===============================================================================================
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = Pi * denom * denom;

    return nom / max(denom, 0.001);
}
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// ================================================================================================
// Fresnel with Schlick's approximation functions:
// http://blog.selfshadow.com/publications/s2013-shading-course/rad/s2013_pbs_rad_notes.pdf
// http://graphicrants.blogspot.fr/2013/08/specular-brdf-reference.html
// ================================================================================================
float3 Schlick_Fresnel(float3 f0, float cosTheta)
{
    return f0 + (float3(1.0f - f0.r, 1.0f - f0.g, 1.0f - f0.b)) * pow((1.0f - cosTheta), 5.0f);
}
float3 Schlick_Fresnel_UE(float3 baseReflectivity, float cosTheta)
{
    return max(baseReflectivity + float3(1.0 - baseReflectivity.x, 1.0 - baseReflectivity.y, 1.0 - baseReflectivity.z) * pow(2, (-5.55473 * cosTheta - 6.98316) * cosTheta), 0.0);
}
float3 Schlick_Fresnel_Roughness(float cosTheta, float3 F0, float roughness)
{
    float a = 1.0f - roughness;
    return F0 + (max(float3(a, a, a), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

static float3 cellProbesPositions[NUM_OF_PROBES_PER_CELL];
static float3 cellProbesSamples[NUM_OF_PROBES_PER_CELL];
static bool cellProbesExistanceFlags[NUM_OF_PROBES_PER_CELL];
// ==============================================================================================================
// Trilinear interpolation of probes in a cell (7 lerps) based on "Irradiance Volumes for Games" by N. Tatarchuk.
// There are some extra checks for edge cases (i.e., if the probe is culled, then don't lerp).
// In theory, it is possible to get rid of 'ifs' and optimize this function but readability will be lost.
// ==============================================================================================================
float3 GetTrilinearInterpolationFromNeighbourProbes(float3 pos, float distanceBetweenDiffuseProbes, float volumeProbesIndexSkip)
{
    float3 result = float3(0.0, 0.0, 0.0);
    
    float cellDistance = distanceBetweenDiffuseProbes * volumeProbesIndexSkip;
    float distanceX0 = abs(cellProbesPositions[0].x - pos.x) / cellDistance;
    float distanceY0 = abs(cellProbesPositions[0].y - pos.y) / cellDistance;
    float distanceZ0 = abs(cellProbesPositions[0].z - pos.z) / cellDistance;
    
    // bottom-left, bottom-right, upper-left, upper-right, bottom-total, upper-total
    bool sideHasColor[6] = { true, true, true, true, true, true };
    
    float3 bottomLeft = float3(0.0, 0.0, 0.0);
    if (cellProbesExistanceFlags[0] && cellProbesExistanceFlags[1])
        bottomLeft = lerp(cellProbesSamples[0], cellProbesSamples[1], distanceZ0);
    else if (cellProbesExistanceFlags[0] && !cellProbesExistanceFlags[1])
        bottomLeft = cellProbesSamples[0];
    else if (!cellProbesExistanceFlags[0] && cellProbesExistanceFlags[1])
        bottomLeft = cellProbesSamples[1];
    else
        sideHasColor[0] = false;
        
    float3 bottomRight = float3(0.0, 0.0, 0.0);
    if (cellProbesExistanceFlags[2] && cellProbesExistanceFlags[3])
        bottomRight = lerp(cellProbesSamples[2], cellProbesSamples[3], distanceZ0);
    else if (cellProbesExistanceFlags[2] && !cellProbesExistanceFlags[3])
        bottomRight = cellProbesSamples[2];
    else if (!cellProbesExistanceFlags[2] && cellProbesExistanceFlags[3])
        bottomRight = cellProbesSamples[3];
    else
        sideHasColor[1] = false;
        
    float3 upperLeft = float3(0.0, 0.0, 0.0);
    if (cellProbesExistanceFlags[4] && cellProbesExistanceFlags[5])
        upperLeft = lerp(cellProbesSamples[4], cellProbesSamples[5], distanceZ0);
    else if (cellProbesExistanceFlags[4] && !cellProbesExistanceFlags[5])
        upperLeft = cellProbesSamples[4];
    else if (!cellProbesExistanceFlags[4] && cellProbesExistanceFlags[5])
        upperLeft = cellProbesSamples[5];
    else
        sideHasColor[2] = false;
    
    float3 upperRight = float3(0.0, 0.0, 0.0);
    if (cellProbesExistanceFlags[6] && cellProbesExistanceFlags[7])
        upperRight = lerp(cellProbesSamples[6], cellProbesSamples[7], distanceZ0);
    else if (cellProbesExistanceFlags[6] && !cellProbesExistanceFlags[7])
        upperRight = cellProbesSamples[6];
    else if (!cellProbesExistanceFlags[6] && cellProbesExistanceFlags[7])
        upperRight = cellProbesSamples[7];
    else
        sideHasColor[3] = false;
    
    float3 bottomTotal = float3(0.0, 0.0, 0.0);
    if (sideHasColor[0] && sideHasColor[1])
        bottomTotal = lerp(bottomLeft, bottomRight, distanceX0);
    else if (sideHasColor[0] && !sideHasColor[1])
        bottomTotal = bottomLeft;
    else if (!sideHasColor[0] && sideHasColor[1])
        bottomTotal = bottomRight;
    else
        sideHasColor[4] = false;
    
    float3 upperTotal = float3(0.0, 0.0, 0.0);
    if (sideHasColor[2] && sideHasColor[3])
        upperTotal = lerp(upperLeft, upperRight, distanceX0);
    else if (sideHasColor[2] && !sideHasColor[3])
        upperTotal = upperLeft;
    else if (!sideHasColor[2] && sideHasColor[3])
        upperTotal = upperRight;
    else
        sideHasColor[5] = false;
    
    if (sideHasColor[4] && sideHasColor[5])
        result = lerp(bottomTotal, upperTotal, distanceY0);
    else if (sideHasColor[4] && !sideHasColor[5])
        result = bottomTotal;
    else if (!sideHasColor[4] && sideHasColor[5])
        result = upperTotal;
    
    return result;
}

// ==============================================================================================================
// Calculate probes volume cascade index by doing a simple world point/aabb collision check
// ==============================================================================================================
int GetProbesVolumeCascade(float3 worldPos, float4 probesVolumeSizes[NUM_OF_PROBE_VOLUME_CASCADES], float3 camPos)
{
    for (int volumeIndex = 0; volumeIndex < NUM_OF_PROBE_VOLUME_CASCADES; volumeIndex++)
    {
        float3 aMax = probesVolumeSizes[volumeIndex].xyz + camPos.xyz;
        float3 aMin = -probesVolumeSizes[volumeIndex].xyz + camPos.xyz;
        
        if ((worldPos.x <= aMax.x && worldPos.x >= aMin.x) &&
			(worldPos.y <= aMax.y && worldPos.y >= aMin.y) &&
			(worldPos.z <= aMax.z && worldPos.z >= aMin.z))
            return volumeIndex;
    }
    
    return -1;
}

// ==============================================================================================================
// Calculate probes cell index (fast uniform-grid searching approach)
// WARNING: can not do multiple indices per pos. (i.e., when pos. is on the edge of several cells)
// ==============================================================================================================
int GetLightProbesCellIndex(float3 pos, float4 probesCellsCount, float3 sceneProbeBounds, float distanceBetweenProbes, float probeSkips)
{
    int finalIndex = -1;
    float3 index = (pos - sceneProbeBounds.xyz) / (distanceBetweenProbes * probeSkips);
    if (index.x < 0.0f || index.x > probesCellsCount.x)
        return -1;
    if (index.y < 0.0f || index.y > probesCellsCount.y)
        return -1;
    if (index.z < 0.0f || index.z > probesCellsCount.z)
        return -1;
    
	//hacky way to prevent from out-of-bounds
    if (index.x == probesCellsCount.x)
        index.x = probesCellsCount.x - 1;
    if (index.y == probesCellsCount.y)
        index.y = probesCellsCount.y - 1;
    if (index.z == probesCellsCount.z)
        index.z = probesCellsCount.z - 1;

    finalIndex = floor(index.y) * (probesCellsCount.x * probesCellsCount.z) + floor(index.x) * probesCellsCount.z + floor(index.z);

    if (finalIndex >= probesCellsCount.w)
        return -1;
    
    return finalIndex;
}

// ==============================================================================================================
// Direct lighting (diffuse & specular) (PBR)
// ==============================================================================================================
float3 DirectDiffuseBRDF(float3 diffuseAlbedo, float3 lightDir, float3 viewDir, float3 F0, float metallic, float3 halfVec)
{
    float3 F = Schlick_Fresnel(F0, max(dot(halfVec, viewDir), 0.0));
    float3 kD = float3(1.0f, 1.0f, 1.0f) - F;
    kD *= (1.0f - metallic);
    
    return (diffuseAlbedo * kD) / Pi;
}
float3 DirectSpecularBRDF(float3 normalWS, float3 lightDir, float3 viewDir, float roughness, float3 F0, float3 halfVec)
{
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(normalWS, halfVec, roughness);
    float G = GeometrySmith(normalWS, viewDir, lightDir, roughness);
    float3 F = Schlick_Fresnel(F0, max(dot(halfVec, viewDir), 0.0));
           
    float3 nominator = NDF * G * F;
    float denominator = 4 * max(dot(normalWS, viewDir), 0.0) * max(dot(normalWS, lightDir), 0.0);
    float3 specular = nominator / max(denominator, 0.001);
       
    return specular;
}
float3 DirectLightingPBR(float3 normalWS, float4 lightColor, float3 lightDir, float3 diffuseAlbedo,
	float3 positionWS, float roughness, float3 F0, float metallic, float3 camPos)
{
    float3 viewDir = normalize(camPos.xyz - positionWS).rgb;
    float3 halfVec = normalize(viewDir + lightDir);
    
    float nDotL = max(dot(normalWS, lightDir), 0.0);
   
    float3 lighting = DirectDiffuseBRDF(diffuseAlbedo, lightDir, viewDir, F0, metallic, halfVec) + DirectSpecularBRDF(normalWS, lightDir, viewDir, roughness, F0, halfVec);

    float lightIntensity = lightColor.a;
    return max(lighting, 0.0f) * nDotL * lightColor.xyz * lightIntensity;
}


// ==============================================================================================================
// Indirect lighting (diffuse & specular) (PBR light probes: diffuse & specular)
// ==============================================================================================================

// ====================================================================================================================
// Get diffuse irradiance from probes:
// 1) probes volume cascade is calculated for the position
// 2) probes cell index is calculated for the cascade
// 3) 8 probes (or less if out of the current cascade) are sampled from IrradianceDiffuseProbesTextureArrays
// 4) Final probe samples are trilinearly interpolated and the final diffuse result is returned
// ====================================================================================================================
float3 GetDiffuseIrradiance(float3 worldPos, float3 normal, float3 camPos, bool useGlobalDiffuseProbe, in SamplerState linearSampler,
    in LightProbeInfo probesInfo)
{
    float3 finalSum = float3(0.0, 0.0, 0.0);
    if (useGlobalDiffuseProbe)
        return probesInfo.globalIrradianceDiffuseProbeTexture.SampleLevel(linearSampler, normal, 0).rgb;
    
    int volumeCascadeIndex = GetProbesVolumeCascade(worldPos, probesInfo.diffuseProbesVolumeSizes, camPos);
    if (volumeCascadeIndex == -1)
        return probesInfo.globalIrradianceDiffuseProbeTexture.SampleLevel(linearSampler, normal, 0).rgb;
    
    int diffuseProbesCellIndex = GetLightProbesCellIndex(worldPos, probesInfo.diffuseProbeCellsCount[volumeCascadeIndex], probesInfo.sceneLightProbeBounds.xyz,
        probesInfo.distanceBetweenDiffuseProbes, probesInfo.volumeProbesIndexSkips[volumeCascadeIndex].r);
    if (diffuseProbesCellIndex != -1)
    {
        for (int i = 0; i < NUM_OF_PROBES_PER_CELL; i++)
        {
            int currentIndex = -1;
            cellProbesExistanceFlags[i] = false;
            
            // get cell's probe global index 
            if (volumeCascadeIndex == 0)
                currentIndex = probesInfo.DiffuseProbesCellsWithProbeIndices0[NUM_OF_PROBES_PER_CELL * diffuseProbesCellIndex + i];
            else if (volumeCascadeIndex == 1)
                currentIndex = probesInfo.DiffuseProbesCellsWithProbeIndices1[NUM_OF_PROBES_PER_CELL * diffuseProbesCellIndex + i];
            
            // get global probes-texture array index for current probe's global index 
            int indexInTexArray = -1;
            if (currentIndex != -1)
            {
                if (volumeCascadeIndex == 0)
                    indexInTexArray = probesInfo.DiffuseProbesTextureArrayIndices0[currentIndex]; // -1 is culled and not in texture array
                else if (volumeCascadeIndex == 1)
                    indexInTexArray = probesInfo.DiffuseProbesTextureArrayIndices1[currentIndex]; // -1 is culled and not in texture array
            }
            // sample probe from global probes-texture array
            cellProbesSamples[i] = float3(0.0, 0.0, 0.0);
            if (indexInTexArray != -1)
            {
                cellProbesExistanceFlags[i] = true;
                if (volumeCascadeIndex == 0)
                    cellProbesSamples[i] = probesInfo.IrradianceDiffuseProbesTextureArray0.SampleLevel(linearSampler, float4(normal, indexInTexArray / 6), 0).rgb;
                else if (volumeCascadeIndex == 1)
                    cellProbesSamples[i] = probesInfo.IrradianceDiffuseProbesTextureArray1.SampleLevel(linearSampler, float4(normal, indexInTexArray / 6), 0).rgb;
            }

            cellProbesPositions[i] = probesInfo.DiffuseProbesPositions[currentIndex];
        }
        
        finalSum = GetTrilinearInterpolationFromNeighbourProbes(worldPos, probesInfo.distanceBetweenDiffuseProbes, probesInfo.volumeProbesIndexSkips[volumeCascadeIndex].r);
    }
    return finalSum;
}

// ====================================================================================================================
// Get specular irradiance from probes:
// 1) probes volume cascade is calculated for the position
// 2) probes cell index is calculated for the cascade
// 3) 8 probes (or less if out of the current cascade) are processed and the closest is found
// 4) Final probe with the closest distance to the position is sampled from 'IrradianceSpecularProbesTextureArray'
// ====================================================================================================================
float3 GetSpecularIrradiance(float3 worldPos, float3 camPos, float3 reflectDir, int mipIndex, bool useGlobalSpecularProbe, in SamplerState linearSampler,
    in LightProbeInfo probesInfo)
{
    float3 finalSum = float3(0.0, 0.0, 0.0);
    if (useGlobalSpecularProbe)
        return probesInfo.globalIrradianceSpecularProbeTexture.SampleLevel(linearSampler, reflectDir, mipIndex).rgb;
    
    int volumeCascadeIndex = GetProbesVolumeCascade(worldPos, probesInfo.specularProbesVolumeSizes, camPos);
    if (volumeCascadeIndex == -1)
        return probesInfo.globalIrradianceSpecularProbeTexture.SampleLevel(linearSampler, reflectDir, mipIndex).rgb;
    
    int specularProbesCellIndex = GetLightProbesCellIndex(worldPos, probesInfo.specularProbeCellsCount[volumeCascadeIndex], probesInfo.sceneLightProbeBounds.xyz,
        probesInfo.distanceBetweenSpecularProbes, probesInfo.volumeProbesIndexSkips[volumeCascadeIndex].r);
    if (specularProbesCellIndex != -1)
    {
        int closestProbeTexArrayIndex = -1;
        int closestProbeDistance = FLT_MAX;
        for (int i = 0; i < NUM_OF_PROBES_PER_CELL; i++)
        {
            int currentIndex = -1;
            
            // get cell's probe global index 
            if (volumeCascadeIndex == 0)
                currentIndex = probesInfo.SpecularProbesCellsWithProbeIndices0[NUM_OF_PROBES_PER_CELL * specularProbesCellIndex + i];
            else if (volumeCascadeIndex == 1)
                currentIndex = probesInfo.SpecularProbesCellsWithProbeIndices1[NUM_OF_PROBES_PER_CELL * specularProbesCellIndex + i];
            
            // get global probes-texture array index for current probe's global index 
            int indexInTexArray = -1;
            if (currentIndex != -1)
            {
                if (volumeCascadeIndex == 0)
                    indexInTexArray = probesInfo.SpecularProbesTextureArrayIndices0[currentIndex]; // -1 is culled and not in texture array
                else if (volumeCascadeIndex == 1)
                    indexInTexArray = probesInfo.SpecularProbesTextureArrayIndices1[currentIndex]; // -1 is culled and not in texture array
            }
            
            // calculate the probe with the closest distance to the position
            if (indexInTexArray != -1)
            {
                float curDistance = distance(worldPos, probesInfo.SpecularProbesPositions[currentIndex]);
                if (curDistance < closestProbeDistance)
                {
                    closestProbeDistance = curDistance;
                    closestProbeTexArrayIndex = indexInTexArray;
                }
            }
        }
        
        if (closestProbeTexArrayIndex != -1)
        {
            if (volumeCascadeIndex == 0)
                finalSum = probesInfo.IrradianceSpecularProbesTextureArray0.SampleLevel(linearSampler, float4(reflectDir, closestProbeTexArrayIndex / 6), mipIndex).rgb;
            else if (volumeCascadeIndex == 1)
                finalSum = probesInfo.IrradianceSpecularProbesTextureArray1.SampleLevel(linearSampler, float4(reflectDir, closestProbeTexArrayIndex / 6), mipIndex).rgb;
        }
    }
    return finalSum;
}

float3 IndirectLightingPBR(float3 normalWS, float3 diffuseAlbedo, float3 positionWS, float roughness, float3 F0, float metalness, float3 camPos, bool useGlobalProbe,
    in LightProbeInfo probesInfo, in SamplerState linearSampler, in Texture2D<float4> integrationTexture, float ambientOcclusion)
{
    float3 viewDir = normalize(camPos.xyz - positionWS);
    float nDotV = max(dot(normalWS, viewDir), 0.0);
    float3 reflectDir = normalize(reflect(-viewDir, normalWS));
    
    //indirect diffuse
    float3 irradianceDiffuse = GetDiffuseIrradiance(positionWS, normalWS, camPos, useGlobalProbe, linearSampler, probesInfo) / Pi;
    float3 indirectDiffuseLighting = irradianceDiffuse * diffuseAlbedo * float3(1.0f - metalness, 1.0f - metalness, 1.0f - metalness);

    //indirect specular (split sum approximation http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf)
    float mipIndex = roughness * SPECULAR_PROBE_MIP_COUNT;
    float3 prefilteredColor = GetSpecularIrradiance(positionWS, camPos, reflectDir, mipIndex, useGlobalProbe, linearSampler, probesInfo);
    float2 environmentBRDF = integrationTexture.SampleLevel(linearSampler, float2(nDotV, roughness), 0).rg;
    float3 indirectSpecularLighting = prefilteredColor * (Schlick_Fresnel_Roughness(nDotV, F0, roughness) * environmentBRDF.x + float3(environmentBRDF.y, environmentBRDF.y, environmentBRDF.y));
    
    return (indirectDiffuseLighting + indirectSpecularLighting) * ambientOcclusion;
}

float3 GetGammaCorrectColor(float3 inputColor)
{
    float factor = 1.0f / 2.2f;
    return pow(inputColor, float3(factor, factor, factor));
}