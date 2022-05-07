// ================================================================================================
// Vertex/Pixel shader for debugging light probes 
//
// Supports:
// - Instancing
//
// Written by Gen Afanasev for 'EveryRay Rendering Engine', 2017-2022
// ================================================================================================
#include "..\\Lighting.hlsli"

cbuffer DebugLightProbeCBuffer : register(b0)
{
    float4x4 ViewProjection;
    float4x4 World;
    float4 CameraPosition;
    float2 DiscardCulled_IsDiffuse;
}
TextureCubeArray<float4> CubemapTexture : register(t0); //only for specular (diffuse use SH)
StructuredBuffer<float3> SphericalHarmonicsCoefficientsArray : register(t1); //linear array of all diffuse probes 2nd order coefficients (9)

SamplerState LinearSampler : register(s0);

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
    float3 Normal : Normal;
    float3 Tangent : Tangent;
    float CullingFlag : TexCoord1; // 1.0f - culled, 0.0f - not culled
    float CubemapIndex : TexCoord2; //index to sample from TextureCubeArray
};


VS_OUTPUT VSMain(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;

    OUT.Position = mul(IN.Position, mul(World, ViewProjection));
    OUT.WorldPos = mul(IN.Position, World).xyz;
    OUT.UV = IN.Texcoord0;
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), World).xyz);
    OUT.Tangent = IN.Tangent;
    OUT.CullingFlag = 0.0f;
    OUT.CubemapIndex = 0.0f;

    return OUT;
}

VS_OUTPUT VSMain_instancing(VS_INPUT_INSTANCING IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;

    float4x4 correctWorld = IN.World;
    correctWorld[0][0] = 1.0f; // since we dont need scale
    
    OUT.WorldPos = mul(IN.Position, correctWorld).xyz;
    OUT.Position = mul(float4(OUT.WorldPos, 1.0f), ViewProjection);
    OUT.UV = IN.Texcoord0;
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), correctWorld).xyz);
    OUT.Tangent = IN.Tangent;
    OUT.CullingFlag = IN.World[3][3];
    OUT.CubemapIndex = IN.World[0][0];

    return OUT;
}

float4 PSMain(VS_OUTPUT vsOutput) : SV_Target0
{
    float3 viewDir = normalize(CameraPosition.xyz - vsOutput.WorldPos);
    float3 reflectDir = normalize(reflect(-viewDir, vsOutput.Normal));
   
    if (vsOutput.CullingFlag > 0.0f)
    {
        if (DiscardCulled_IsDiffuse.r > 0.0f)
            discard;
        else
            return float4(0.5f, 0.5f, 0.5f, 1.0f);
    }
    if (DiscardCulled_IsDiffuse.g > 0.0)
    {
        float3 SH[SPHERICAL_HARMONICS_ORDER * SPHERICAL_HARMONICS_ORDER];
        for (int i = 0; i < SPHERICAL_HARMONICS_ORDER * SPHERICAL_HARMONICS_ORDER; i++)
            SH[i] = SphericalHarmonicsCoefficientsArray[vsOutput.CubemapIndex + i];
        
        return float4(GetDiffuseIrradianceFromSphericalHarmonics(vsOutput.Normal, SH) / Pi, 1.0f);
    }
    else
        return float4(CubemapTexture.Sample(LinearSampler, float4(reflectDir, vsOutput.CubemapIndex)).rgb, 1.0f);
}

float3 PSMain_recompute(VS_OUTPUT vsOutput) : SV_Target0
{
    float3 color = float3(1.0f, 0.0f, 0.0f);
    return color;
}