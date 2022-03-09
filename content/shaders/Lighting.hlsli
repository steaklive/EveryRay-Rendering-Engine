#define FLT_MAX 3.402823466e+38

#define NUM_OF_SHADOW_CASCADES 3
#define NUM_OF_PROBES_PER_CELL 8
#define NUM_OF_PROBE_VOLUME_CASCADES 2
#define SPECULAR_PROBE_MIP_COUNT 6

static const float4 ColorWhite = { 1, 1, 1, 1 };
static const float Pi = 3.141592654f;

//for probe GI
static float3 cellProbesPositions[NUM_OF_PROBES_PER_CELL];
static float3 cellProbesSamples[NUM_OF_PROBES_PER_CELL];
static bool cellProbesExistanceFlags[NUM_OF_PROBES_PER_CELL];

// ================================================================================================
// Simple PCF cascaded shadow mapping
float CalculateShadow(float3 worldPos, float4x4 svp, int index, float ShadowTexelSize, Texture2D<float> CascadedShadowTextures[NUM_OF_SHADOW_CASCADES], SamplerComparisonState CascadedPcfShadowMapSampler)
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
        2.0 * CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy, ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d2, d1), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d1, -d2), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d2, -d1), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d1, d2), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d4, d3), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d3, -d4), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d4, -d3), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d3, d4), ShadowCoord.z)
        ) / 10.0;

    return result * result;
}
float GetShadow(float4 worldPos, float4x4 ShadowMatrices[NUM_OF_SHADOW_CASCADES], float4 ShadowCascadeDistances, float ShadowTexelSize, Texture2D<float> CascadedShadowTextures[NUM_OF_SHADOW_CASCADES], SamplerComparisonState CascadedPcfShadowMapSampler)
{
    float depthDistance = worldPos.a;
    
    if (depthDistance < ShadowCascadeDistances.x)
        return CalculateShadow(worldPos.rgb, ShadowMatrices[0], 0, ShadowTexelSize, CascadedShadowTextures, CascadedPcfShadowMapSampler);
    else if (depthDistance < ShadowCascadeDistances.y)
        return CalculateShadow(worldPos.rgb, ShadowMatrices[1], 1, ShadowTexelSize, CascadedShadowTextures, CascadedPcfShadowMapSampler);
    else if (depthDistance < ShadowCascadeDistances.z)
        return CalculateShadow(worldPos.rgb, ShadowMatrices[2], 2, ShadowTexelSize, CascadedShadowTextures, CascadedPcfShadowMapSampler);
    else
        return 1.0f;
}

// ===============================================================================================
// PBR functions (GGX)
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