// ================================================================================================
// Vertex/Pixel shader for fresnel outline effect. 
//
// Written by Gen Afanasev for 'EveryRay Rendering Engine', 2017-2022
// ================================================================================================

cbuffer FresnelOutlineCBuffer : register (b0)
{
    float4x4 ViewProjection;
    float4 CameraPosition;
    float4 FresnelColorExp; //rgb - color, a - exponent
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
    OUT.WorldPos = mul(IN.Position, World).xyz;
    OUT.UV = IN.Texcoord0;
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), World).xyz);
    OUT.Tangent = normalize(mul(float4(IN.Tangent, 0), World).xyz);
    OUT.ViewDir = IN.Position.xyz - CameraPosition.xyz;
    return OUT;
}

VS_OUTPUT VSMain_instancing(VS_INPUT_INSTANCING IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT)0;

    OUT.WorldPos = mul(IN.Position, IN.World).xyz;

    OUT.Position = mul(float4(OUT.WorldPos, 1.0f), ViewProjection);
    OUT.UV = IN.Texcoord0;
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), IN.World).xyz);
    OUT.Tangent = normalize(mul(float4(IN.Tangent, 0), IN.World).xyz);
    OUT.ViewDir = IN.Position.xyz - CameraPosition.xyz;
    return OUT;
}

float4 PSMain(VS_OUTPUT vsOutput) : SV_Target
{ 
    float2 texCoord = vsOutput.UV;
    
    float3 viewDir = normalize(CameraPosition - vsOutput.WorldPos);
    float fresnel = dot(vsOutput.Normal, viewDir);
    fresnel = saturate(1 - fresnel);
    fresnel = pow(fresnel, FresnelColorExp.a);

    float3 fresnelColor = fresnel * FresnelColorExp.rgb;
    return float4(fresnelColor, fresnel);
}