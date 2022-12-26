// ================================================================================================
// Vertex/Pixel shader for simple "normals" snow. 
//
// Written by Gen Afanasev for 'EveryRay Rendering Engine', 2017-2022
// ================================================================================================
#include "Lighting.hlsli"
#include "Common.hlsli"

Texture2D<float4> SnowAlbedoTexture : register(t0);
Texture2D<float4> SnowNormalTexture : register(t1);
Texture2D<float4> SnowRoughnessTexture : register(t2);
Texture2D<float4> NormalTexture : register(t3);
Texture2D<float> CascadedShadowTextures[NUM_OF_SHADOW_CASCADES] : register(t4);

SamplerState SamplerLinear : register(s0);
SamplerComparisonState CascadedPcfShadowMapSampler : register(s1);

static const float3 snowDir = { 0.0, 1.0, 0.0 };

cbuffer SnowCBuffer : register (b0)
{
    float4x4 ShadowMatrices[NUM_OF_SHADOW_CASCADES];
    float4x4 ViewProjection;
    float4 ShadowTexelSize;
    float4 ShadowCascadeDistances;
    float4 SunDirection;
    float4 SunColor;
    float4 CameraPosition;
    float4 SnowDepthLevelUVScale; // x - depth, y - level, z - uv scale
}

cbuffer ObjectCBuffer : register(b1)
{
    float4x4 World;
    float UseGlobalProbe;
    float SkipIndirectProbeLighting;
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
};

VS_OUTPUT VSMain(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT)0;

    OUT.Position = mul(IN.Position, mul(World, ViewProjection));
    if (dot(IN.Normal, snowDir) >= SnowDepthLevelUVScale.y)
        OUT.Position.xyz += snowDir.xyz * SnowDepthLevelUVScale.x;

    OUT.WorldPos = mul(IN.Position, World).xyz;
    OUT.UV = IN.Texcoord0;
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), World).xyz);
    OUT.Tangent = normalize(mul(float4(IN.Tangent, 0), World).xyz);
    OUT.ViewDir = IN.Position.xyz - CameraPosition.xyz;
    OUT.ShadowCoord0 = mul(IN.Position, mul(World, ShadowMatrices[0])).xyz;
    OUT.ShadowCoord1 = mul(IN.Position, mul(World, ShadowMatrices[1])).xyz;
    OUT.ShadowCoord2 = mul(IN.Position, mul(World, ShadowMatrices[2])).xyz;

    return OUT;
}

VS_OUTPUT VSMain_instancing(VS_INPUT_INSTANCING IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT)0;

    OUT.WorldPos = mul(IN.Position, IN.World).xyz;

    OUT.Position = mul(float4(OUT.WorldPos, 1.0f), ViewProjection);
    if (dot(IN.Normal, snowDir) >= SnowDepthLevelUVScale.y)
        OUT.Position.xyz += snowDir.xyz * SnowDepthLevelUVScale.x;

    OUT.UV = IN.Texcoord0;
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), IN.World).xyz);
    OUT.Tangent = normalize(mul(float4(IN.Tangent, 0), IN.World).xyz);
    OUT.ViewDir = IN.Position.xyz - CameraPosition.xyz;
    OUT.ShadowCoord0 = mul(IN.Position, mul(IN.World, ShadowMatrices[0])).xyz;
    OUT.ShadowCoord1 = mul(IN.Position, mul(IN.World, ShadowMatrices[1])).xyz;
    OUT.ShadowCoord2 = mul(IN.Position, mul(IN.World, ShadowMatrices[2])).xyz;

    return OUT;
}

float4 PSMain(VS_OUTPUT vsOutput) : SV_Target
{ 
    float2 texCoord = vsOutput.UV;
    
    float3x3 TBN = float3x3(vsOutput.Tangent, cross(vsOutput.Normal, vsOutput.Tangent), vsOutput.Normal);
    float3 normalWS = float3(0.0, 0.0, 0.0);
    float3 sampledNormal = (2 * NormalTexture.Sample(SamplerLinear, texCoord).xyz) - 1.0;
    sampledNormal = mul(sampledNormal, TBN);
    normalWS = normalize(sampledNormal);

    if (dot(normalWS, snowDir) >= SnowDepthLevelUVScale.y)
    {
        float4 diffuseAlbedoNonGamma = SnowAlbedoTexture.Sample(SamplerLinear, SnowDepthLevelUVScale.z * texCoord);
        clip(diffuseAlbedoNonGamma.a < 0.000001f ? -1 : 1);
        float3 diffuseAlbedo = pow(diffuseAlbedoNonGamma.rgb, 2.2);
        
        float3 snowNormal = 2.0f * SnowNormalTexture.Sample(SamplerLinear, SnowDepthLevelUVScale.z * texCoord).rgb - 1.0f;
        normalWS = normalize(snowNormal + normalWS);
        
        float roughness = SnowRoughnessTexture.Sample(SamplerLinear, SnowDepthLevelUVScale.z * texCoord).r;

        float3 F0 = float3(0.04, 0.04, 0.04);
        float3 directLighting = DirectLightingPBR(normalWS, SunColor, SunDirection.xyz, diffuseAlbedo.rgb, vsOutput.WorldPos, roughness, F0, 0.0f, CameraPosition.xyz);

        float3 indirectLighting = float3(0.1f, 0.1f, 0.1f) * diffuseAlbedo;
       
        float3 shadowCoords[3] = { vsOutput.ShadowCoord0, vsOutput.ShadowCoord1, vsOutput.ShadowCoord2 };
        float shadow = Forward_GetShadow(ShadowCascadeDistances, shadowCoords, ShadowTexelSize.r, CascadedShadowTextures, CascadedPcfShadowMapSampler, vsOutput.Position.w, -1);
        float3 color = directLighting * shadow + indirectLighting;

        return float4(color, 1.0f);
    }

    discard;
    return float4(0.0, 0.0, 0.0, 0.0);

}