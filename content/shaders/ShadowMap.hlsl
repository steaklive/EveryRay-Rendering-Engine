// ================================================================================================
// Vertex/Pixel shader for rendering depth into shadow map RT
// 
// Supports:
// - instancing
//
// Written by Gen Afanasev for 'EveryRay Rendering Engine', 2017-2022
// ================================================================================================

#include "IndirectCulling.hlsli"
#include "Common.hlsli"

cbuffer ShadowMapCBuffer : register(b0)
{
    float4x4 WorldLightViewProjection;
    float4x4 LightViewProjection; // for not breaking the legacy code...
}
// register(b1) is objects cbuffer from Common.hlsli

struct VS_INPUT
{
    float4 Position : POSITION;
    float2 TextureCoordinate : TEXCOORD;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
};
    
struct VS_INPUT_INSTANCING
{
    float4 Position : POSITION;
    float2 TextureCoordinate : TEXCOORD;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    
    //instancing;
    row_major float4x4 World : WORLD; // not used with indirect rendering
    uint InstanceID : SV_InstanceID;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float2 Depth : TexCoord0;
    float2 TextureCoordinate : TexCoord1;
};

Texture2D<float4> AlbedoTexture : register(t0);
StructuredBuffer<Instance> IndirectInstanceData : register(t1);

SamplerState Sampler : register(s0);

VS_OUTPUT VSMain(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;

    OUT.Position = mul(IN.Position, WorldLightViewProjection);
    OUT.Depth = OUT.Position.zw;
    OUT.TextureCoordinate = IN.TextureCoordinate;

    return OUT;
}
VS_OUTPUT VSMain_instancing(VS_INPUT_INSTANCING IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;

    float4x4 World = IsIndirectlyRendered > 0.0 ?
        transpose(IndirectInstanceData[(int)OriginalInstanceCount * (int)CurrentLod + IN.InstanceID].WorldMat) : IN.World;
    float3 WorldPos = mul(IN.Position, World).xyz;
    OUT.Position = mul(float4(WorldPos, 1.0f), LightViewProjection);
    OUT.Depth = OUT.Position.zw;
    OUT.TextureCoordinate = IN.TextureCoordinate;
    
    return OUT;
}

float4 PSMain(VS_OUTPUT IN) : SV_Target
{
    float alphaValue = AlbedoTexture.Sample(Sampler, IN.TextureCoordinate).a;
    
    if (alphaValue > 0.1f)
        IN.Depth.x /= IN.Depth.y;
    else
        discard;

    return float4(IN.Depth.x, 0, 0, 1);
}