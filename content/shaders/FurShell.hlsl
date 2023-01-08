// ================================================================================================
// Vertex/Pixel shader for fur shell rendering. 
//
// Written by Gen Afanasev for 'EveryRay Rendering Engine', 2017-2023
// ================================================================================================
#include "Lighting.hlsli"
#include "Common.hlsli"

Texture2D<float4> FurAlbedoTexture : register(t0);
Texture2D<float> FurHeightTexture : register(t1);
Texture2D<float> FurMaskTexture : register(t2);

Texture2D<float> CascadedShadowTextures[NUM_OF_SHADOW_CASCADES] : register(t3);

SamplerState SamplerLinear : register(s0);
SamplerComparisonState CascadedPcfShadowMapSampler : register(s1);

cbuffer FurShellCBuffer : register (b0)
{
    float4x4 ShadowMatrices[NUM_OF_SHADOW_CASCADES];
    float4x4 ViewProjection;
    float4 ShadowTexelSize;
    float4 ShadowCascadeDistances;
    float4 SunDirection;
    float4 SunColor;
    float4 CameraPosition;
    float4 FurColor;
    float4 FurGravityStrength; // xyz - direction, w - strength
    float4 FurLengthCutoffCutoffEndFade; // x - length, y - cutoff ("thickness"), z - cutoff end ("thickness at the ends"), w - edge fade 
    float4 FurMultiplierUVScale; // x - multiplier, y - uv scale
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

    float3 direction = lerp(IN.Normal, FurGravityStrength.xyz * FurGravityStrength.w + IN.Normal * (1.0f - FurGravityStrength.w), FurMultiplierUVScale.x);
    float3 p = IN.Position + direction * FurLengthCutoffCutoffEndFade.x * FurMultiplierUVScale.x;
    OUT.Position = mul(float4(p, 1.0f), mul(World, ViewProjection));

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

    float3 direction = lerp(IN.Normal, FurGravityStrength.xyz * FurGravityStrength.w + IN.Normal * (1.0f - FurGravityStrength.w), FurMultiplierUVScale.x);
    float3 p = IN.Position + direction * FurLengthCutoffCutoffEndFade.x * FurMultiplierUVScale.x;
    OUT.WorldPos = mul(p, IN.World).xyz;

    OUT.Position = mul(float4(OUT.WorldPos, 1.0f), ViewProjection);
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
    float2 texCoord = vsOutput.UV * FurMultiplierUVScale.y;
    float furHeight = FurHeightTexture.Sample(SamplerLinear, texCoord).r;
    float furMask = FurMaskTexture.Sample(SamplerLinear, texCoord).r;
    float3 furAlbedoNonGamma = FurAlbedoTexture.Sample(SamplerLinear, texCoord).rgb;
    float3 furAlbedo = pow(furAlbedoNonGamma.rgb, 2.2);

    if (furMask > 0.0)
    {
        float finalAlpha = step(lerp(FurLengthCutoffCutoffEndFade.y, FurLengthCutoffCutoffEndFade.z, FurMultiplierUVScale.x), furHeight);
        float alpha = 1 - (FurMultiplierUVScale.x * FurMultiplierUVScale.x);
        //alpha += dot(vsOutput.ViewDir.xyz, vsOutput.Normal) - FurLengthCutoffCutoffEndFade.w;
        finalAlpha *= alpha;

        float3 shadowCoords[3] = { vsOutput.ShadowCoord0, vsOutput.ShadowCoord1, vsOutput.ShadowCoord2 };
        float shadow = Forward_GetShadow(ShadowCascadeDistances, shadowCoords, ShadowTexelSize.r, CascadedShadowTextures, CascadedPcfShadowMapSampler, vsOutput.Position.w, -1);

        float3 directLighting = furHeight * lerp(furAlbedo, FurColor.xyz, FurColor.a) * SunColor * shadow * max(dot(SunDirection.xyz, vsOutput.Normal), 0.0);
        float3 indirectLighting = float3(0.1f, 0.1f, 0.1f) * furAlbedo; //fake ambient

        return float4(directLighting + indirectLighting, finalAlpha);
    }
    else
        return float4(0.0, 0.0, 0.0, 0.0);
}