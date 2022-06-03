#include "..\Common.hlsli"

static const int TILE_SIZE = 512;
static const int DETAIL_TEXTURE_REPEAT = 32;

cbuffer TerrainDataCBuffer : register(b0)
{
    float4x4 ShadowMatrices[NUM_OF_SHADOW_CASCADES];
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
    float4 SunDirection;
    float4 SunColor;
    float4 ShadowTexelSize;
    float4 ShadowCascadeDistances;
    float4 CameraPosition;
    float TessellationFactor;
    float TerrainHeightScale;
    float UseDynamicTessellation;
    float TessellationFactorDynamic;
    float DistanceFactor;
};

struct VS_INPUT_TS
{
    float4 PatchInfo : PATCH_INFO;
};

struct HS_INPUT
{
    float2 origin : ORIGIN;
    float2 size : SIZE;
};

struct HS_OUTPUT
{
    float Dummmy : DUMMY;
};

struct DS_OUTPUT
{
    float4 position : SV_Position;
    centroid float2 texcoord : TEXCOORD0;
    centroid float3 normal : NORMAL;
    float3 shadowCoord0 : TEXCOORD2;
    float3 shadowCoord1 : TEXCOORD3;
    float3 shadowCoord2 : TEXCOORD4;
};

struct PatchData
{
    float Edges[4] : SV_TessFactor;
    float Inside[2] : SV_InsideTessFactor;

    float2 origin : ORIGIN;
    float2 size : SIZE;
};

SamplerState LinearSampler : register(s0);
SamplerState LinearSamplerClamp : register(s1);
SamplerComparisonState CascadedPcfShadowMapSampler : register(s2);

Texture2D<float> heightTexture : register(t0);
Texture2D<float4> splatTexture : register(t1);
Texture2D<float4> channel0Texture : register(t2);
Texture2D<float4> channel1Texture : register(t3);
Texture2D<float4> channel2Texture : register(t4);
Texture2D<float4> channel3Texture : register(t5);
Texture2D<float> cascadedShadowTextures[NUM_OF_SHADOW_CASCADES] : register(t6);

HS_INPUT VSMain(VS_INPUT_TS IN)
{
    HS_INPUT OUT = (HS_INPUT) 0;
	
    OUT.origin = IN.PatchInfo.xy;
    OUT.size = IN.PatchInfo.zw;
    return OUT;
}

float CalculateShadow(float3 ShadowCoord, int index)
{
    const float Dilation = 2.0;
    float d1 = Dilation * ShadowTexelSize.x * 0.125;
    float d2 = Dilation * ShadowTexelSize.x * 0.875;
    float d3 = Dilation * ShadowTexelSize.x * 0.625;
    float d4 = Dilation * ShadowTexelSize.x * 0.375;
    float result = (
        2.0 *
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy, ShadowCoord.z) +
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d2, d1), ShadowCoord.z) +
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d1, -d2), ShadowCoord.z) +
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d2, -d1), ShadowCoord.z) +
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d1, d2), ShadowCoord.z) +
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d4, d3), ShadowCoord.z) +
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d3, -d4), ShadowCoord.z) +
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d4, -d3), ShadowCoord.z) +
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d3, d4), ShadowCoord.z)
        ) / 10.0;

    return result * result;
}
float GetShadow(float3 ShadowCoord0, float3 ShadowCoord1, float3 ShadowCoord2, float depthDistance)
{
    if (depthDistance < ShadowCascadeDistances.x)
        return CalculateShadow(ShadowCoord0, 0);
    else if (depthDistance < ShadowCascadeDistances.y)
        return CalculateShadow(ShadowCoord1, 1);
    else if (depthDistance < ShadowCascadeDistances.z)
        return CalculateShadow(ShadowCoord2, 2);
    else
        return 1.0f;
    
}

// hyperbolic function that determines a factor
float GetTessellationFactorFromCamera(float distance)
{
    return lerp(TessellationFactor, TessellationFactorDynamic * (1 / (DistanceFactor * distance)), UseDynamicTessellation);
}

// https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/chapter5-andersson-terrain-rendering-in-frostbite.pdf
float3 GetNormalFromHeightmap(float2 uv, float texelSize, float maxHeight)
{
    float4 h;
    h[0] = heightTexture.SampleLevel(LinearSampler, uv + texelSize * float2(0, -1), 0).r * maxHeight;
    h[1] = heightTexture.SampleLevel(LinearSampler, uv + texelSize * float2(-1, 0), 0).r * maxHeight;
    h[2] = heightTexture.SampleLevel(LinearSampler, uv + texelSize * float2(1, 0), 0).r * maxHeight;
    h[3] = heightTexture.SampleLevel(LinearSampler, uv + texelSize * float2(0, 1), 0).r * maxHeight;
    
    float3 n;
    n.z = h[0] - h[3];
    n.x = h[1] - h[2];
    n.y = 2;

    return normalize(n);
}

PatchData hull_constant_function(InputPatch<HS_INPUT, 1> inputPatch)
{
    PatchData output;

    float distance_to_camera = 0.0f;
    float tesselation_factor = 0.0f;
    float inside_tessellation_factor = 0.0f;
    //float in_frustum = 0;

    output.origin = inputPatch[0].origin;
    output.size = inputPatch[0].size;
    
    float4 pos = float4(inputPatch[0].origin.x, 0.0f, inputPatch[0].origin.y, 1.0f);
    pos = mul(pos, World);

    distance_to_camera = length(CameraPosition.xz - pos.xz - float2(0, inputPatch[0].size.y * 0.5));
    tesselation_factor = GetTessellationFactorFromCamera(distance_to_camera);
    output.Edges[0] = tesselation_factor;
    inside_tessellation_factor += tesselation_factor;

    distance_to_camera = length(CameraPosition.xz - pos.xz - float2(inputPatch[0].size.x * 0.5, 0));
    tesselation_factor = GetTessellationFactorFromCamera(distance_to_camera);
    output.Edges[1] = tesselation_factor;
    inside_tessellation_factor += tesselation_factor;

    distance_to_camera = length(CameraPosition.xz - pos.xz - float2(inputPatch[0].size.x, inputPatch[0].size.y * 0.5));
    tesselation_factor = GetTessellationFactorFromCamera(distance_to_camera);
    output.Edges[2] = tesselation_factor;
    inside_tessellation_factor += tesselation_factor;

    distance_to_camera = length(CameraPosition.xz - pos.xz - float2(inputPatch[0].size.x * 0.5, inputPatch[0].size.y));
    tesselation_factor = GetTessellationFactorFromCamera(distance_to_camera);
    output.Edges[3] = tesselation_factor;
    inside_tessellation_factor += tesselation_factor;

    output.Inside[0] = output.Inside[1] = inside_tessellation_factor * 0.25;


    return output;
}

[domain("quad")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(1)]
[patchconstantfunc("hull_constant_function")]
HS_OUTPUT HSMain(InputPatch<HS_INPUT, 1> inputPatch)
{
    return (HS_OUTPUT)0;
}

[domain("quad")]
DS_OUTPUT DSMain(PatchData input, float2 uv : SV_DomainLocation, OutputPatch<HS_OUTPUT, 1> inputPatch)
{
    DS_OUTPUT output;
    float3 vertexPosition;
    
    float2 texcoord01 = (input.origin + uv * input.size) / float(TILE_SIZE);
    float height = heightTexture.SampleLevel(LinearSamplerClamp, texcoord01, 0).r;
	
    vertexPosition.xz = input.origin + uv * input.size;
    vertexPosition.y = TerrainHeightScale * height;
   
    //calculating base normal rotation matrix
    //float3 normal = normalize(/*2.0f **/ normalTexture.SampleLevel(TerrainSplatSampler, float2(texcoord01.x, 1.0f - texcoord01.y), 0).rgb /*- float3(1.0f, 1.0f, 1.0f)*/);
    //normal.z = -normal.z;
    //float3x3 normal_rotation_matrix;
    //normal_rotation_matrix[1] = normal;
    //normal_rotation_matrix[2] = normalize(cross(float3(-1.0, 0.0, 0.0), normal_rotation_matrix[1]));
    //normal_rotation_matrix[0] = normalize(cross(normal_rotation_matrix[2], normal_rotation_matrix[1]));

	//applying base rotation matrix to detail normal
    //float3 normalRot = mul(normal, normal_rotation_matrix);
    
	// writing output params
    output.position = mul(float4(vertexPosition, 1.0), World);
    output.position = mul(output.position, View);
    output.position = mul(output.position, Projection);
    output.texcoord = texcoord01;
    output.normal = float3(0, 0, 0);
    output.shadowCoord0 = mul(float4(vertexPosition, 1.0), mul(World, ShadowMatrices[0])).xyz;
    output.shadowCoord1 = mul(float4(vertexPosition, 1.0), mul(World, ShadowMatrices[1])).xyz;
    output.shadowCoord2 = mul(float4(vertexPosition, 1.0), mul(World, ShadowMatrices[2])).xyz;
    return output;
}

float4 PSMain(DS_OUTPUT IN) : SV_Target
{   
    float2 uvTile = IN.texcoord;
    
    //float4 height = heightTexture.Sample(TerrainHeightSampler, uvTile);
    float3 normal = GetNormalFromHeightmap(uvTile, 1.0f / (float) (TILE_SIZE), TerrainHeightScale);
   
    uvTile.y = 1.0f - uvTile.y;
    
    float4 splat = splatTexture.Sample(LinearSampler, uvTile);
    float3 channel0 = channel0Texture.Sample(LinearSampler, uvTile * DETAIL_TEXTURE_REPEAT).rgb;
    float3 channel1 = channel1Texture.Sample(LinearSampler, uvTile * DETAIL_TEXTURE_REPEAT).rgb;
    float3 channel2 = channel2Texture.Sample(LinearSampler, uvTile * DETAIL_TEXTURE_REPEAT).rgb;
    float3 channel3 = channel3Texture.Sample(LinearSampler, uvTile * DETAIL_TEXTURE_REPEAT).rgb;
    
    float3 albedo = splat.r * channel0 + splat.g * channel1 + splat.b * channel2 + splat.a * channel3;
    
    float3 color = float3(0.1f, 0.1f, 0.1f);//AmbientColor.rgb;

    float shadow = GetShadow(IN.shadowCoord0, IN.shadowCoord1, IN.shadowCoord2, IN.position.w);
    shadow = clamp(shadow, 0.5f, 1.0f);
    
    float lightIntensity = saturate(dot(normal, SunDirection.rgb));

    if (lightIntensity > 0.0f)
        color += SunColor.rgb * lightIntensity;

    color = saturate(color);
    color *= albedo * shadow;
    
    return float4(color, 1.0f);
}