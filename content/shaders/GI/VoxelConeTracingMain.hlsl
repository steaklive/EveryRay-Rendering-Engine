#define PI 3.141592654
#define NUM_VOXEL_CONES 6
#define NUM_SHADOW_CASCADES 3
#define NUM_VOXEL_CASCADES 2

#define DEBUG_CASCADE_BLENDING 0
#define DEBUG_SKIPPED_VOXELS 0

static const float4 colorWhite = { 1, 1, 1, 1 };
static const float coneAperture = 0.577f; // 6 cones, 60deg each, tan(30deg) = aperture
static const float3 diffuseConeDirections[] =
{
    float3(0.0f, 1.0f, 0.0f),
    float3(0.0f, 0.5f, 0.866025f),
    float3(0.823639f, 0.5f, 0.267617f),
    float3(0.509037f, 0.5f, -0.7006629f),
    float3(-0.50937f, 0.5f, -0.7006629f),
    float3(-0.823639f, 0.5f, 0.267617f)
};
static const float diffuseConeWeights[] =
{
    0.25, 0.15, 0.15, 0.15, 0.15, 0.15
};

static const float specularOneDegree = 0.0174533f; //in radians
static const int specularMaxDegreesCount = 2;

Texture2D<float4> albedoBuffer : register(t0);
Texture2D<float4> normalBuffer : register(t1);
Texture2D<float4> worldPosBuffer : register(t2);
Texture2D<float4> extraGBuffer : register(t3);
Texture3D<float4> voxelTextures[NUM_VOXEL_CASCADES] : register(t4);

RWTexture2D<float4> outputTexture : register(u0);

SamplerState LinearSampler : register(s0);

cbuffer VCTMainCB : register(b0)
{
    float4 VoxelCameraPositions[NUM_VOXEL_CASCADES];
    float4 WorldVoxelScales[NUM_VOXEL_CASCADES];
    float4 CameraPos;
    float2 UpsampleRatio;
    float IndirectDiffuseStrength;
    float IndirectSpecularStrength;
    float MaxConeTraceDistance;
    float AOFalloff;
    float SamplingFactor;
    float VoxelSampleOffset;
    float GIPower;
    float PreviousRadianceDelta;
    float2 pad1;
};

float4 GetVoxel(float3 worldPosition, float3 weight, float lod, int cascadeIndex, int cascadeResolution)
{   
    float3 voxelTextureUV = (worldPosition - VoxelCameraPositions[cascadeIndex].xyz) / (0.5f * (float) cascadeResolution) * WorldVoxelScales[cascadeIndex].r;
    voxelTextureUV.y = -voxelTextureUV.y;
    voxelTextureUV = voxelTextureUV * 0.5f + 0.5f + float3(VoxelSampleOffset, VoxelSampleOffset, VoxelSampleOffset);
    return voxelTextures[cascadeIndex].SampleLevel(LinearSampler, voxelTextureUV, lod);
}

float4 TraceCone(float3 pos, float3 normal, float3 direction, float aperture, out float occlusion, bool calculateAO, int cascadeIndex, int cascadeResolution)
{
    float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);
    occlusion = 0.0f;
    
    float voxelWorldSize = 1.0f / WorldVoxelScales[cascadeIndex].r;
    float dist = voxelWorldSize;
    float3 startPos = pos + normal * dist;
    
    float3 weight = direction * direction;

    while (dist < MaxConeTraceDistance && color.a < 1.0f)
    {
        float diameter = 2.0f * aperture * dist;
        float lodLevel = log2(diameter / voxelWorldSize);
        float4 voxelColor = GetVoxel(startPos + dist * direction, weight, lodLevel, cascadeIndex, cascadeResolution);
    
        // front-to-back
        color += (1.0 - color.a) * voxelColor;
        if (occlusion < 1.0f && calculateAO)
            occlusion += ((1.0f - occlusion) * voxelColor.a) / (1.0f + AOFalloff * diameter);
        
        dist += diameter * SamplingFactor;
    }

    return color;
}

float4 CalculateIndirectSpecular(float3 worldPos, float3 normal, float4 specular)
{
    float4 result = float4(0.0, 0.0, 0.0, 1.0);
    float3 viewDirection = normalize(CameraPos.rgb - worldPos);
    float3 coneDirection = normalize(reflect(-viewDirection, normal));
        
    float aperture = clamp(tan(PI * 0.5 * specular.a), specularOneDegree * specularMaxDegreesCount, PI);
    float ao = -1.0f;
    
    uint voxelCascadeResolutions[NUM_VOXEL_CASCADES], empty1, empty2;
    for (int cascadeIndex = 0; cascadeIndex < NUM_VOXEL_CASCADES; cascadeIndex++)
    {
        voxelTextures[cascadeIndex].GetDimensions(voxelCascadeResolutions[cascadeIndex], empty1, empty2);

        float shift = voxelCascadeResolutions[cascadeIndex] / WorldVoxelScales[cascadeIndex].r * 0.5f;
        float3 voxelGridBoundsMax = VoxelCameraPositions[cascadeIndex].xyz + float3(shift, shift, shift);
        float3 voxelGridBoundsMin = VoxelCameraPositions[cascadeIndex].xyz - float3(shift, shift, shift);
        
        if (worldPos.x < voxelGridBoundsMin.x || worldPos.y < voxelGridBoundsMin.y || worldPos.z < voxelGridBoundsMin.z ||
            worldPos.x > voxelGridBoundsMax.x || worldPos.y > voxelGridBoundsMax.y || worldPos.z > voxelGridBoundsMax.z)
            continue; //try to trace from next cascade
        else
            result += TraceCone(worldPos, normal, coneDirection, aperture, ao, false, cascadeIndex, voxelCascadeResolutions[cascadeIndex]);
    }
    
    return IndirectSpecularStrength * result * float4(specular.rgb, 1.0f) * specular.a;
}

float4 CalculateIndirectDiffuse(float3 worldPos, float3 normal, out float ao)
{
    float4 totalRadiance = float4(0.0, 0.0, 0.0, 1.0);
    float totalAo = 0.0f;
    float tempAo = 0.0f;
    float3 coneDirection;
    
    float3 upDir = float3(0.0f, 1.0f, 0.0f);
    if (abs(dot(normal, upDir)) == 1.0f)
        upDir = float3(0.0f, 0.0f, 1.0f);

    float3 right = normalize(upDir - dot(normal, upDir) * normal);
    float3 up = cross(right, normal);
       
    uint voxelCascadeResolutions[NUM_VOXEL_CASCADES], empty1, empty2;
    for (int cascadeIndex = NUM_VOXEL_CASCADES - 1; cascadeIndex >= 0; cascadeIndex--)
    {
        voxelTextures[cascadeIndex].GetDimensions(voxelCascadeResolutions[cascadeIndex], empty1, empty2);

        float4 currentCascadeRadiance = float4(0.0, 0.0, 0.0, 1.0);
        float currentCascadeAo = 0.0f;

        const float shift = voxelCascadeResolutions[cascadeIndex] / WorldVoxelScales[cascadeIndex].r * 0.5f;
        const float3 voxelGridBoundsMax = VoxelCameraPositions[cascadeIndex].xyz + float3(shift, shift, shift);
        const float3 voxelGridBoundsMin = VoxelCameraPositions[cascadeIndex].xyz - float3(shift, shift, shift);

        // if prev cascade (bigger) didn't trace anything < margin in that position, we skip re-tracing as an optimization
        const bool canSkipVoxel = (cascadeIndex < NUM_VOXEL_CASCADES - 1) && (totalRadiance.x < PreviousRadianceDelta && totalRadiance.y < PreviousRadianceDelta && totalRadiance.z < PreviousRadianceDelta);

#if DEBUG_SKIPPED_VOXELS
        if (canSkipVoxel)
            totalRadiance = float4(1.0, 0.0, 1.0, 1.0);
#endif

        float distToCenter = distance(worldPos.xyz, VoxelCameraPositions[cascadeIndex].xyz);
        float distBetweenBounds = abs(voxelGridBoundsMax.x - voxelGridBoundsMin.x);

        if (worldPos.x < voxelGridBoundsMin.x || worldPos.y < voxelGridBoundsMin.y || worldPos.z < voxelGridBoundsMin.z ||
            worldPos.x > voxelGridBoundsMax.x || worldPos.y > voxelGridBoundsMax.y || worldPos.z > voxelGridBoundsMax.z || canSkipVoxel)
            continue; //try to trace from next cascade
        else
        {
            for (int i = 0; i < NUM_VOXEL_CONES; i++)
            {
                coneDirection = normal;
                coneDirection += diffuseConeDirections[i].x * right + diffuseConeDirections[i].z * up;
                coneDirection = normalize(coneDirection);
        
                currentCascadeRadiance += TraceCone(worldPos, normal, coneDirection, coneAperture, tempAo, true, cascadeIndex, voxelCascadeResolutions[cascadeIndex]) * diffuseConeWeights[i];
                currentCascadeAo += tempAo * diffuseConeWeights[i];
            }

            if (cascadeIndex == NUM_VOXEL_CASCADES - 1)
            {
#if DEBUG_CASCADE_BLENDING
                totalRadiance += float4(1.0, 0.0, 0.0, 1.0);
#else
                totalRadiance += currentCascadeRadiance;
#endif
                totalAo += currentCascadeAo;
            }
            else
            {
#if DEBUG_CASCADE_BLENDING
                currentCascadeRadiance = float4(0.0, 0.0, 0.0, 1.0);
#endif
                float blendFactor = distToCenter / (0.5f * distBetweenBounds); // how much to blend from the bounds of the cascade: the further from the center, the more you blend with bigger cascades
                totalAo = lerp(currentCascadeAo, totalAo, blendFactor);
                totalRadiance = lerp(currentCascadeRadiance,totalRadiance, blendFactor);
            }
        }
    }

    ao = totalAo;
    return IndirectDiffuseStrength * totalRadiance;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    uint width, height;
    albedoBuffer.GetDimensions(width, height);
    
    float2 texCooord = (DTid.xy + 0.5f) / float2(width / UpsampleRatio.x, height / UpsampleRatio.y);
    
    float3 normal = normalize(normalBuffer.SampleLevel(LinearSampler, texCooord, 0).rgb);
    float4 worldPos = worldPosBuffer.SampleLevel(LinearSampler, texCooord, 0);
    float4 albedo = albedoBuffer.SampleLevel(LinearSampler, texCooord, 0);
    float4 extraGbuffer = extraGBuffer.SampleLevel(LinearSampler, texCooord, 0);
       
    float ao = 0.0f;
    float4 indirectDiffuse = CalculateIndirectDiffuse(worldPos.rgb, normal.rgb, ao);
    //float4 indirectSpecular = CalculateIndirectSpecular(worldPos.rgb, normal.rgb, float4(albedo.rgb, extraGbuffer.g));

    outputTexture[DTid.xy] = GIPower * saturate(float4(indirectDiffuse.rgb * albedo.rgb /*+ indirectSpecular.rgb*/, ao));
}