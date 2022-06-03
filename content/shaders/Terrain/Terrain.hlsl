// ================================================================================================
// Vertex/Domain/Hull/Pixel shader for terrain rendering 
//
// Supports:
// - Cascaded Shadow Mapping
// - PBR with Image Based Lighting (via global light probes)
// - dynamic GPU tessellation
//
// TODO:
// - move to "Forward+"
// - add support for point/spot lights
// - add support for ambient occlusion
//
// Written by Gen Afanasev for 'EveryRay Rendering Engine', 2017-2022
// ================================================================================================


#include "..\Common.hlsli"
#include "..\Lighting.hlsli"

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
    float4 worldPos : TEXCOORD0;
    centroid float2 texcoord : TEXCOORD1;
    centroid float3 normal : NORMAL;
    float3 shadowCoord0 : TEXCOORD3;
    float3 shadowCoord1 : TEXCOORD4;
    float3 shadowCoord2 : TEXCOORD5;
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

Texture2D<float4> SplatTexture : register(t0);
Texture2D<float4> Channel0Texture : register(t1);
Texture2D<float4> Channel1Texture : register(t2);
Texture2D<float4> Channel2Texture : register(t3);
Texture2D<float4> Channel3Texture : register(t4);
Texture2D<float> CascadedShadowTextures[NUM_OF_SHADOW_CASCADES] : register(t5);

// here go some GI probe textures from Lighting.hlsli (from t8 to t17), thats why we start from t18 

Texture2D<float> HeightTexture : register(t18);

HS_INPUT VSMain(VS_INPUT_TS IN)
{
    HS_INPUT OUT = (HS_INPUT) 0;
	
    OUT.origin = IN.PatchInfo.xy;
    OUT.size = IN.PatchInfo.zw;
    return OUT;
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
    h[0] = HeightTexture.SampleLevel(LinearSampler, uv + texelSize * float2(0, -1), 0).r * maxHeight;
    h[1] = HeightTexture.SampleLevel(LinearSampler, uv + texelSize * float2(-1, 0), 0).r * maxHeight;
    h[2] = HeightTexture.SampleLevel(LinearSampler, uv + texelSize * float2(1, 0), 0).r * maxHeight;
    h[3] = HeightTexture.SampleLevel(LinearSampler, uv + texelSize * float2(0, 1), 0).r * maxHeight;
    
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
    float height = HeightTexture.SampleLevel(LinearSamplerClamp, texcoord01, 0).r;
	
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
    output.worldPos = output.position;
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
    
    float4 splat = SplatTexture.Sample(LinearSampler, uvTile);
    float3 channel0 = Channel0Texture.Sample(LinearSampler, uvTile * DETAIL_TEXTURE_REPEAT).rgb;
    float3 channel1 = Channel1Texture.Sample(LinearSampler, uvTile * DETAIL_TEXTURE_REPEAT).rgb;
    float3 channel2 = Channel2Texture.Sample(LinearSampler, uvTile * DETAIL_TEXTURE_REPEAT).rgb;
    float3 channel3 = Channel3Texture.Sample(LinearSampler, uvTile * DETAIL_TEXTURE_REPEAT).rgb;
    float3 diffuseAlbedo = splat.r * channel0 + splat.g * channel1 + splat.b * channel2 + splat.a * channel3;

    float3 shadowCoords[3] = { IN.shadowCoord0, IN.shadowCoord1, IN.shadowCoord2 };
    float shadow = Forward_GetShadow(ShadowCascadeDistances, shadowCoords, ShadowTexelSize.r, CascadedShadowTextures, CascadedPcfShadowMapSampler, IN.position.w, -1);
    
    float roughness = 1.0f;
    float metalness = 0.0f;
    float ao = 1.0f;
    
    LightProbeInfo probesInfo;
    probesInfo.globalIrradianceDiffuseProbeTexture = DiffuseGlobalProbeTexture;
    probesInfo.globalIrradianceSpecularProbeTexture = SpecularGlobalProbeTexture;
    
    float3 directLighting = DirectLightingPBR(normal, SunColor, SunDirection.xyz, diffuseAlbedo.rgb, IN.worldPos.xyz, roughness, float3(0.04, 0.04, 0.04), metalness, CameraPosition.xyz);
    float3 indirectLighting = IndirectLightingPBR(normal, diffuseAlbedo.rgb, IN.worldPos.xyz, roughness, float3(0.04, 0.04, 0.04), metalness, CameraPosition.xyz, true, probesInfo, LinearSampler, IntegrationTexture, ao);
    return float4(directLighting * shadow + indirectLighting, 1.0f);
}