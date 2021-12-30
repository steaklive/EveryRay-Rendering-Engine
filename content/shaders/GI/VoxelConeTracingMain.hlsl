#define PI 3.141592654
#define NUM_VOXEL_CONES 6
#define NUM_SHADOW_CASCADES 3
#define NUM_VOXEL_CASCADES 2

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

Texture2DMS<float4> albedoBuffer : register(t0);
Texture2DMS<float4> normalBuffer : register(t1);
Texture2DMS<float4> worldPosBuffer : register(t2);
Texture2DMS<float4> extraGBuffer : register(t3);
Texture3D<float4> voxelTextures[NUM_VOXEL_CASCADES] : register(t4);

RWTexture2D<float4> outputTexture : register(u0);

SamplerState LinearSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = BORDER;
    AddressV = BORDER;
    AddressW = BORDER;
    BorderColor = colorWhite;
};

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
    float3 pad1;
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
    float4 result = float4(0.0, 0.0, 0.0, 1.0);
    float3 coneDirection;
    
    float3 upDir = float3(0.0f, 1.0f, 0.0f);
    if (abs(dot(normal, upDir)) == 1.0f)
        upDir = float3(0.0f, 0.0f, 1.0f);

    float3 right = normalize(upDir - dot(normal, upDir) * normal);
    float3 up = cross(right, normal);
    
    float finalAo = 0.0f;
    float tempAo = 0.0f;
    
    uint voxelCascadeResolutions[NUM_VOXEL_CASCADES], empty1, empty2;
    for (int cascadeIndex = 0; cascadeIndex < NUM_VOXEL_CASCADES; cascadeIndex++)
        voxelTextures[cascadeIndex].GetDimensions(voxelCascadeResolutions[cascadeIndex], empty1, empty2);
    
    for (int i = 0; i < NUM_VOXEL_CONES; i++)
    {
        coneDirection = normal;
        coneDirection += diffuseConeDirections[i].x * right + diffuseConeDirections[i].z * up;
        coneDirection = normalize(coneDirection);

        for (int cascadeIndex = 0; cascadeIndex < NUM_VOXEL_CASCADES; cascadeIndex++)
        {
            float shift = voxelCascadeResolutions[cascadeIndex] / WorldVoxelScales[cascadeIndex].r * 0.5f;
            float3 voxelGridBoundsMax = VoxelCameraPositions[cascadeIndex].xyz + float3(shift, shift, shift);
            float3 voxelGridBoundsMin = VoxelCameraPositions[cascadeIndex].xyz - float3(shift, shift, shift);
        
            if (worldPos.x < voxelGridBoundsMin.x || worldPos.y < voxelGridBoundsMin.y || worldPos.z < voxelGridBoundsMin.z ||
                worldPos.x > voxelGridBoundsMax.x || worldPos.y > voxelGridBoundsMax.y || worldPos.z > voxelGridBoundsMax.z)
                continue; //try to trace from next cascade
            else
            {
                result += TraceCone(worldPos, normal, coneDirection, coneAperture, tempAo, true, cascadeIndex, voxelCascadeResolutions[cascadeIndex]) * diffuseConeWeights[i];
                finalAo += tempAo * diffuseConeWeights[i];
            }
        }
    }
    
    ao = finalAo;
    
    return IndirectDiffuseStrength * result;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    float2 inPos = DTid.xy;
    
    float3 normal = normalize(normalBuffer.Load(inPos * UpsampleRatio, 0).rgb);
    float4 worldPos = worldPosBuffer.Load(inPos * UpsampleRatio, 0);
    float4 albedo = albedoBuffer.Load(inPos * UpsampleRatio, 0);
    float4 extraGbuffer = extraGBuffer.Load(inPos * UpsampleRatio, 0);
       
    float ao = 0.0f;
    float4 indirectDiffuse = CalculateIndirectDiffuse(worldPos.rgb, normal.rgb, ao);
    //float4 indirectSpecular = CalculateIndirectSpecular(worldPos.rgb, normal.rgb, float4(albedo.rgb, extraGbuffer.g));

    outputTexture[inPos] = GIPower * saturate(float4(indirectDiffuse.rgb * albedo.rgb /*+ indirectSpecular.rgb*/, ao));
}