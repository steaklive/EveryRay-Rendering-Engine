// ================================================================================================
// Compute shader for deferred direct and indirect lighting. 
// Contains cascaded shadow mapping, light probes and PBR support
// 
// Written by Gen Afanasev for 'EveryRay Rendering Engine', 2017-2022
// ================================================================================================

#define FLT_MAX 3.402823466e+38
#define NUM_OF_SHADOW_CASCADES 3
#define NUM_OF_PROBES_PER_CELL 8
#define NUM_OF_PROBE_VOLUME_CASCADES 2
#define SPECULAR_PROBE_MIP_COUNT 6

static const float4 ColorWhite = { 1, 1, 1, 1 };
static const float Pi = 3.141592654f;

static float3 cellProbesPositions[NUM_OF_PROBES_PER_CELL];
static float3 cellProbesSamples[NUM_OF_PROBES_PER_CELL];
static bool cellProbesExistanceFlags[NUM_OF_PROBES_PER_CELL];

SamplerState SamplerLinear : register(s0);
SamplerComparisonState CascadedPcfShadowMapSampler : register(s1);

RWTexture2D<float4> OutputTexture : register(u0);

Texture2D<float4> GbufferAlbedoTexture : register(t0);
Texture2D<float4> GbufferNormalTexture : register(t1);
Texture2D<float4> GbufferWorldPosTexture : register(t2);
Texture2D<float4> GbufferExtraTexture : register(t3); // [reflection mask, roughness, metalness, foliage mask]
Texture2D<float4> GbufferExtra2Texture : register(t4); // [global diffuse probe mask, empty, empty, empty]

TextureCubeArray<float4> IrradianceDiffuseProbesTextureArray0 : register(t5); //cascade 0
TextureCubeArray<float4> IrradianceDiffuseProbesTextureArray1 : register(t6); //cascade 1
TextureCube<float4> IrradianceDiffuseGlobalProbeTexture : register(t7); // global probe
TextureCubeArray<float4> IrradianceSpecularProbesTextureArray0 : register(t8); //cascade 0
TextureCubeArray<float4> IrradianceSpecularProbesTextureArray1 : register(t9); //cascade 1

Texture2D<float4> IntegrationTexture : register(t10);

Texture2D<float> CascadedShadowTextures[NUM_OF_SHADOW_CASCADES] : register(t11);

StructuredBuffer<int> DiffuseProbesCellsWithProbeIndices0 : register(t14); //cascade 0, linear array of cells with NUM_OF_PROBES_PER_CELL probes' indices in each cell
StructuredBuffer<int> DiffuseProbesCellsWithProbeIndices1 : register(t15); //cascade 1, linear array of cells with NUM_OF_PROBES_PER_CELL probes' indices in each cell
StructuredBuffer<int> DiffuseProbesTextureArrayIndices0 : register(t16); //cascade 0, array of all diffuse probes in scene with indices in 'IrradianceDiffuseProbesTextureArray' for each probe (-1 if not in array)
StructuredBuffer<int> DiffuseProbesTextureArrayIndices1 : register(t17); //cascade 1, array of all diffuse probes in scene with indices in 'IrradianceDiffuseProbesTextureArray' for each probe (-1 if not in array)
StructuredBuffer<float3> DiffuseProbesPositions : register(t18); //linear array of all diffuse probes positions

StructuredBuffer<int> SpecularProbesCellsWithProbeIndices0 : register(t19); //cascade 0, linear array of cells with NUM_OF_PROBES_PER_CELL probes' indices in each cell
StructuredBuffer<int> SpecularProbesCellsWithProbeIndices1 : register(t20); //cascade 1, linear array of cells with NUM_OF_PROBES_PER_CELL probes' indices in each cell
StructuredBuffer<int> SpecularProbesTextureArrayIndices0 : register(t21); //cascade 0, array of all specular probes in scene with indices in 'IrradianceSpecularProbesTextureArray' for each probe (-1 if not in array)
StructuredBuffer<int> SpecularProbesTextureArrayIndices1 : register(t22); //cascade 1, array of all specular probes in scene with indices in 'IrradianceSpecularProbesTextureArray' for each probe (-1 if not in array)
StructuredBuffer<float3> SpecularProbesPositions : register(t23); //linear array of all specular probes positions

cbuffer DeferredLightingCBuffer : register(b0)
{
    float4x4 ShadowMatrices[NUM_OF_SHADOW_CASCADES];
    float4 ShadowCascadeDistances;
    float4 ShadowTexelSize;
    float4 SunDirection;
    float4 SunColor;
    float4 CameraPosition;
}

cbuffer LightProbesCBuffer : register(b1)
{
    float4 LightProbesVolumeBounds[NUM_OF_PROBE_VOLUME_CASCADES];
    float4 DiffuseProbesCellsCount[NUM_OF_PROBE_VOLUME_CASCADES]; //x,y,z,total
    float4 DiffuseProbeIndexSkip[NUM_OF_PROBE_VOLUME_CASCADES];
    float4 SpecularProbesCellsCount[NUM_OF_PROBE_VOLUME_CASCADES]; //x,y,z,total
    float4 SpecularProbeIndexSkip[NUM_OF_PROBE_VOLUME_CASCADES];
    float4 SceneLightProbesBounds; //volume's extent of all scene's probes
    float DistanceBetweenDiffuseProbes;
    float DistanceBetweenSpecularProbes;
}

// ================================================================================================
// Simple PCF cascaded shadow mapping
float CalculateShadow(float3 worldPos, float4x4 svp, int index)
{
    float4 lightSpacePos = mul(svp, float4(worldPos, 1.0f));
    float4 ShadowCoord = lightSpacePos / lightSpacePos.w;
    ShadowCoord.rg = ShadowCoord.rg * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    
    const float Dilation = 2.0;
    float d1 = Dilation * ShadowTexelSize.x * 0.125;
    float d2 = Dilation * ShadowTexelSize.x * 0.875;
    float d3 = Dilation * ShadowTexelSize.x * 0.625;
    float d4 = Dilation * ShadowTexelSize.x * 0.375;
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
float GetShadow(float4 worldPos)
{
    float depthDistance = worldPos.a;
    
    if (depthDistance < ShadowCascadeDistances.x)
        return CalculateShadow(worldPos.rgb, ShadowMatrices[0], 0);
    else if (depthDistance < ShadowCascadeDistances.y)
        return CalculateShadow(worldPos.rgb, ShadowMatrices[1], 1);
    else if (depthDistance < ShadowCascadeDistances.z)
        return CalculateShadow(worldPos.rgb, ShadowMatrices[2], 2);
    else
        return 1.0f;   
}

// ===============================================================================================
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

float3 DirectLightingPBR(float3 normalWS, float3 lightColor, float3 diffuseAlbedo,
	float3 positionWS, float roughness, float3 F0, float metallic)
{
    float3 lightDir = SunDirection.xyz;
    float3 viewDir = normalize(CameraPosition.xyz - positionWS).rgb;
    float3 halfVec = normalize(viewDir + lightDir);
    
    float nDotL = max(dot(normalWS, lightDir), 0.0);
   
    float3 lighting = DirectDiffuseBRDF(diffuseAlbedo, lightDir, viewDir, F0, metallic, halfVec) + DirectSpecularBRDF(normalWS, lightDir, viewDir, roughness, F0, halfVec);

    float lightIntensity = SunColor.a;
    return max(lighting, 0.0f) * nDotL * lightColor * lightIntensity;
}


// ==============================================================================================================
// Trilinear interpolation of probes in a cell (7 lerps) based on "Irradiance Volumes for Games" by N. Tatarchuk.
// There are some extra checks for edge cases (i.e., if the probe is culled, then don't lerp).
// In theory, it is possible to get rid of 'ifs' and optimize this function but readability will be lost.
// ==============================================================================================================
float3 GetTrilinearInterpolationFromNeighbourProbes(float3 pos, int volumeIndex)
{
    float3 result = float3(0.0, 0.0, 0.0);
    
    float cellDistance = DistanceBetweenDiffuseProbes * DiffuseProbeIndexSkip[volumeIndex];
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

int GetLightProbesCellIndex(float3 pos, bool isDiffuse, int volumeIndex)
{
    int finalIndex = -1;
    if (isDiffuse)
    {
        float3 index = (pos - SceneLightProbesBounds.xyz) / (DistanceBetweenDiffuseProbes * DiffuseProbeIndexSkip[volumeIndex].x);
        if (index.x < 0.0f || index.x > DiffuseProbesCellsCount[volumeIndex].x)
            return -1;
        if (index.y < 0.0f || index.y > DiffuseProbesCellsCount[volumeIndex].y)
            return -1;
        if (index.z < 0.0f || index.z > DiffuseProbesCellsCount[volumeIndex].z)
            return -1;
        
		//little hacky way to prevent from out-of-bounds
        if (index.x == DiffuseProbesCellsCount[volumeIndex].x)
            index.x = DiffuseProbesCellsCount[volumeIndex].x - 1;
        if (index.y == DiffuseProbesCellsCount[volumeIndex].y)
            index.y = DiffuseProbesCellsCount[volumeIndex].y - 1;
        if (index.z == DiffuseProbesCellsCount[volumeIndex].z)
            index.z = DiffuseProbesCellsCount[volumeIndex].z - 1;

        finalIndex = floor(index.y) * (DiffuseProbesCellsCount[volumeIndex].x * DiffuseProbesCellsCount[volumeIndex].z) + floor(index.x) * DiffuseProbesCellsCount[volumeIndex].z + floor(index.z);

        if (finalIndex >= DiffuseProbesCellsCount[volumeIndex].w)
            return -1;
    }
    else
    {
        float3 index = (pos - SceneLightProbesBounds.xyz) / (DistanceBetweenSpecularProbes * SpecularProbeIndexSkip[volumeIndex].x);
        if (index.x < 0.0f || index.x > SpecularProbesCellsCount[volumeIndex].x)
            return -1;
        if (index.y < 0.0f || index.y > SpecularProbesCellsCount[volumeIndex].y)
            return -1;
        if (index.z < 0.0f || index.z > SpecularProbesCellsCount[volumeIndex].z)
            return -1;
        
		//little hacky way to prevent from out-of-bounds
        if (index.x == SpecularProbesCellsCount[volumeIndex].x)
            index.x = SpecularProbesCellsCount[volumeIndex].x - 1;
        if (index.y == SpecularProbesCellsCount[volumeIndex].y)
            index.y = SpecularProbesCellsCount[volumeIndex].y - 1;
        if (index.z == SpecularProbesCellsCount[volumeIndex].z)
            index.z = SpecularProbesCellsCount[volumeIndex].z - 1;

        finalIndex = floor(index.y) * (SpecularProbesCellsCount[volumeIndex].x * SpecularProbesCellsCount[volumeIndex].z) + floor(index.x) * SpecularProbesCellsCount[volumeIndex].z + floor(index.z);

        if (finalIndex >= SpecularProbesCellsCount[volumeIndex].w)
            return -1;
    }
    
    return finalIndex;
}

int GetProbesVolumeCascade(float3 worldPos)
{
    for (int volumeIndex = 0; volumeIndex < NUM_OF_PROBE_VOLUME_CASCADES; volumeIndex++)
    {
        float3 aMax = LightProbesVolumeBounds[volumeIndex].xyz + CameraPosition.xyz;
        float3 aMin = -LightProbesVolumeBounds[volumeIndex].xyz + CameraPosition.xyz;
        
        if ((worldPos.x <= aMax.x && worldPos.x >= aMin.x) &&
			(worldPos.y <= aMax.y && worldPos.y >= aMin.y) &&
			(worldPos.z <= aMax.z && worldPos.z >= aMin.z))
            return volumeIndex;
    }
    
    return -1;
}

// ====================================================================================================================
// Get diffuse irradiance from probes:
// 1) probes volume cascade is calculated for the position
// 2) probes cell index is calculated for the cascade
// 3) 8 probes (or less if out of the current cascade) are sampled from IrradianceDiffuseProbesTextureArrays
// 4) Final probe samples are trilinearly interpolated and the final diffuse result is returned
// ====================================================================================================================
float3 GetDiffuseIrradiance(float3 worldPos, float3 normal, bool useGlobalDiffuseProbe)
{
    float3 finalSum = float3(1.0, 0.0, 0.0);
    if (useGlobalDiffuseProbe)
        return IrradianceDiffuseGlobalProbeTexture.SampleLevel(SamplerLinear, normal, 0).rgb;
    
    int volumeCascadeIndex = GetProbesVolumeCascade(worldPos);
    if (volumeCascadeIndex == -1)
        return IrradianceDiffuseGlobalProbeTexture.SampleLevel(SamplerLinear, normal, 0).rgb;
    
    int diffuseProbesCellIndex = GetLightProbesCellIndex(worldPos, true, volumeCascadeIndex);
    if (diffuseProbesCellIndex != -1)
    {
        for (int i = 0; i < NUM_OF_PROBES_PER_CELL; i++)
        {
            int currentIndex = -1;
            cellProbesExistanceFlags[i] = false;
            
            // get cell's probe global index 
            if (volumeCascadeIndex == 0)
                currentIndex = DiffuseProbesCellsWithProbeIndices0[NUM_OF_PROBES_PER_CELL * diffuseProbesCellIndex + i];
            else if (volumeCascadeIndex == 1)
                currentIndex = DiffuseProbesCellsWithProbeIndices1[NUM_OF_PROBES_PER_CELL * diffuseProbesCellIndex + i];
            
            // get global probes-texture array index for current probe's global index 
            int indexInTexArray = -1;
            if (volumeCascadeIndex == 0)
                indexInTexArray = DiffuseProbesTextureArrayIndices0[currentIndex]; // -1 is culled and not in texture array
            else if (volumeCascadeIndex == 1)
                indexInTexArray = DiffuseProbesTextureArrayIndices1[currentIndex]; // -1 is culled and not in texture array
            
            // sample probe from global probes-texture array
            cellProbesSamples[i] = float3(0.0, 0.0, 0.0);
            if (indexInTexArray != -1)
            {
                cellProbesExistanceFlags[i] = true;
                if (volumeCascadeIndex == 0)
                    cellProbesSamples[i] = IrradianceDiffuseProbesTextureArray0.SampleLevel(SamplerLinear, float4(normal, indexInTexArray / 6), 0).rgb;
                else if (volumeCascadeIndex == 1)
                    cellProbesSamples[i] = IrradianceDiffuseProbesTextureArray1.SampleLevel(SamplerLinear, float4(normal, indexInTexArray / 6), 0).rgb;
            }

            cellProbesPositions[i] = DiffuseProbesPositions[currentIndex];
        }
        
        finalSum = GetTrilinearInterpolationFromNeighbourProbes(worldPos, volumeCascadeIndex);
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
float3 GetSpecularIrradiance(float3 worldPos, float3 reflectDir, int mipIndex)
{
    float3 finalSum = float3(0.0, 0.0, 0.0);
    
    int volumeCascadeIndex = GetProbesVolumeCascade(worldPos);
    if (volumeCascadeIndex == -1)
        return finalSum;
    
    int specularProbesCellIndex = GetLightProbesCellIndex(worldPos, false, volumeCascadeIndex);
    if (specularProbesCellIndex != -1)
    {
        int closestProbeTexArrayIndex = -1;
        int closestProbeDistance = FLT_MAX;
        for (int i = 0; i < NUM_OF_PROBES_PER_CELL; i++)
        {
            int currentIndex = -1;
            
            // get cell's probe global index 
            if (volumeCascadeIndex == 0)
                currentIndex = SpecularProbesCellsWithProbeIndices0[NUM_OF_PROBES_PER_CELL * specularProbesCellIndex + i];
            else if (volumeCascadeIndex == 1)
                currentIndex = SpecularProbesCellsWithProbeIndices1[NUM_OF_PROBES_PER_CELL * specularProbesCellIndex + i];
            
            // get global probes-texture array index for current probe's global index 
            int indexInTexArray = -1;
            if (volumeCascadeIndex == 0)
                indexInTexArray = SpecularProbesTextureArrayIndices0[currentIndex]; // -1 is culled and not in texture array
            else if (volumeCascadeIndex == 1)
                indexInTexArray = SpecularProbesTextureArrayIndices1[currentIndex]; // -1 is culled and not in texture array
            
            // calculate the probe with the closest distance to the position
            if (indexInTexArray != -1)
            {
                float curDistance = distance(worldPos, SpecularProbesPositions[currentIndex]);
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
                finalSum = IrradianceSpecularProbesTextureArray0.SampleLevel(SamplerLinear, float4(reflectDir, closestProbeTexArrayIndex / 6), mipIndex).rgb;
            else if (volumeCascadeIndex == 1)
                finalSum = IrradianceSpecularProbesTextureArray1.SampleLevel(SamplerLinear, float4(reflectDir, closestProbeTexArrayIndex / 6), mipIndex).rgb;
        }
    }
    return finalSum;
}

float3 IndirectLightingPBR(float3 diffuseAlbedo, float3 normalWS, float3 positionWS, float roughness, float3 F0, float metalness, bool useGlobalDiffuseProbe)
{
    float3 viewDir = normalize(CameraPosition.xyz - positionWS);
    float nDotV = max(dot(normalWS, viewDir), 0.0);
    float3 reflectDir = normalize(reflect(-viewDir, normalWS));
    float ao = 1.0f;

    //indirect diffuse
    float3 irradianceDiffuse = GetDiffuseIrradiance(positionWS, normalWS, useGlobalDiffuseProbe) /* / Pi*/;
    float3 indirectDiffuseLighting = irradianceDiffuse * diffuseAlbedo * float3(1.0f - metalness, 1.0f - metalness, 1.0f - metalness);

    //indirect specular (split sum approximation http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf)
    float3 F = Schlick_Fresnel_UE(F0, nDotV);
    float mipIndex = roughness * SPECULAR_PROBE_MIP_COUNT;
    float3 prefilteredColor = GetSpecularIrradiance(positionWS, reflectDir, mipIndex);
    float2 environmentBRDF = IntegrationTexture.SampleLevel(SamplerLinear, float2(nDotV, roughness), 0).rg;
    float3 indirectSpecularLighting = prefilteredColor * (F * environmentBRDF.x + environmentBRDF.y);
    
    return (indirectDiffuseLighting + indirectSpecularLighting) * ao;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    uint2 inPos = DTid.xy;
    
    float4 worldPos = GbufferWorldPosTexture.Load(uint3(inPos, 0));
    if (worldPos.a < 0.000001f)
        return;
    
    float4 diffuseAlbedo = pow(GbufferAlbedoTexture.Load(uint3(inPos, 0)), 2.2);    
    float3 normalWS = GbufferNormalTexture.Load(uint3(inPos, 0)).rgb;
    
    float4 extraGbuffer = GbufferExtraTexture.Load(uint3(inPos, 0));
    float roughness = extraGbuffer.g;
    float metalness = extraGbuffer.b;
    
    float4 extra2Gbuffer = GbufferExtra2Texture.Load(uint3(inPos, 0));
    bool useGlobalDiffuseProbe = extra2Gbuffer.r > 0.0f;

    //reflectance at normal incidence for dia-electic or metal
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, diffuseAlbedo.rgb, metalness);

    float3 directLighting = float3(0.0, 0.0, 0.0);
       directLighting += DirectLightingPBR(normalWS, SunColor.xyz, diffuseAlbedo.rgb, worldPos.rgb, roughness, F0, metalness);
    
    float3 indirectLighting = float3(0.0, 0.0, 0.0);
    if (extraGbuffer.a < 1.0f)
        indirectLighting += IndirectLightingPBR(diffuseAlbedo.rgb, normalWS, worldPos.rgb, roughness, F0, metalness, useGlobalDiffuseProbe);
    
    float shadow = GetShadow(worldPos);
    
    float3 color = (directLighting * shadow) + indirectLighting;
    color = color / (color + float3(1.0f, 1.0f, 1.0f));

    OutputTexture[inPos] += float4(color, 1.0f);
}