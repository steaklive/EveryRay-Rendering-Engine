// ================================================================================================
// Vertex/Pixel shader for GBuffer rendering. 
//
// Supports:
// - Instancing
//
// Written by Gen Afanasev for 'EveryRay Rendering Engine', 2017-2022
// ================================================================================================

Texture2D<float4> AlbedoMap : register(t0);
Texture2D<float4> NormalMap : register(t1);
Texture2D<float> RoughnessMap : register(t2);
Texture2D<float> MetallicMap : register(t3);
Texture2D<float> HeightMap : register(t4);
Texture2D<float> ReflectionMaskMap : register(t5);

cbuffer GBufferCBuffer : register(b0)
{
    float4x4 ViewProjection;
    float4x4 World;
    float4 Reflection_Foliage_UseGlobalDiffuseProbe_POM_MaskFactor;
    float4 SkipDeferredLighting_UseSSS_CustomAlphaDiscard; // a - empty
    float4 CustomRoughnessMetalnessSkipIndSpec; // r - roughness, g - metalness, b - skip indirect specular
}

SamplerState Sampler : register(s0);

struct VS_INPUT
{
    float4 ObjectPosition : POSITION;
    float2 TextureCoordinate : TEXCOORD;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
};
    
struct VS_INPUT_INSTANCING
{
    float4 ObjectPosition : POSITION;
    float2 TextureCoordinate : TEXCOORD;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    
    //instancing
    row_major float4x4 World : WORLD;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float2 TextureCoordinate : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
};

VS_OUTPUT VSMain(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;
    
    OUT.Position = mul(IN.ObjectPosition, mul(World, ViewProjection));
    OUT.WorldPos = mul(IN.ObjectPosition, World).xyz;
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), World).xyz);
    OUT.TextureCoordinate = IN.TextureCoordinate;
    OUT.Tangent = IN.Tangent;
    
    return OUT;
}
VS_OUTPUT VSMain_instancing(VS_INPUT_INSTANCING IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;
    
    OUT.WorldPos = mul(IN.ObjectPosition, IN.World).xyz;
    OUT.Position = mul(float4(OUT.WorldPos, 1.0f), ViewProjection);
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), IN.World).xyz);
    OUT.TextureCoordinate = IN.TextureCoordinate;
    OUT.Tangent = IN.Tangent;
    
    return OUT;
}

struct PS_OUTPUT
{
    float4 Color : SV_Target0;
    float4 Normal : SV_Target1;
    float4 WorldPos : SV_Target2;
    float4 Extra : SV_Target3;
    float4 Extra2 : SV_Target4;
};

float3x3 invert_3x3(float3x3 M)
{
    float D = determinant(M);
    float3x3 T = transpose(M);

    return float3x3(
        cross(T[1], T[2]),
        cross(T[2], T[0]),
        cross(T[0], T[1])) / (D + 1e-6);
}

PS_OUTPUT PSMain(VS_OUTPUT IN, bool isFrontFace : SV_IsFrontFace) : SV_Target
{
    PS_OUTPUT OUT;
    float4 albedo = AlbedoMap.Sample(Sampler, IN.TextureCoordinate);
    if (albedo.a < SkipDeferredLighting_UseSSS_CustomAlphaDiscard.b)
        discard;
    
    OUT.Color = albedo;
    
    float3 sampledNormal = normalize(2.0f * NormalMap.Sample(Sampler, IN.TextureCoordinate).xyz - float3(1.0, 1.0, 1.0));
 
    // A way of calculating normals without tangent/bitangent
    //float3x3 tbnTransform;
    //float3 dp1 = ddx_fine(IN.WorldPos);
    //float3 dp2 = ddy_fine(IN.WorldPos);
    //float2 duv1 = ddx_fine(IN.TextureCoordinate);
    //float2 duv2 = ddy_fine(IN.TextureCoordinate);
    //float3x3 M = float3x3(dp1, dp2, normalize(IN.Normal));
    //float3x3 inverseM = invert_3x3(M);
    //float3 T = mul(inverseM, float3(duv1.x, duv2.x, 0));
    //float3 B = mul(inverseM, float3(duv1.y, duv2.y, 0));
    //float scaleT = 1.0f / (dot(T, T) + 1e-6);
    //float scaleB = 1.0f / (dot(B, B) + 1e-6);
    //tbnTransform[0] = normalize(T * scaleT);
    //tbnTransform[1] = -normalize(B * scaleB);
    //tbnTransform[2] = normalize(IN.Normal);
    //sampledNormal = ((tbnTransform[0] * sampledNormal.x) + (tbnTransform[1] * sampledNormal.y) + (tbnTransform[2]));

    float3x3 tbn = float3x3(IN.Tangent, cross(IN.Normal, IN.Tangent), IN.Normal);
    sampledNormal = mul(sampledNormal.rgb, tbn); // Transform normal to world space
    
    OUT.Normal = float4(sampledNormal, 1.0);
    OUT.WorldPos = float4(IN.WorldPos, IN.Position.w);
    
    float roughness = CustomRoughnessMetalnessSkipIndSpec.r >= 0.0f ? CustomRoughnessMetalnessSkipIndSpec.r : RoughnessMap.Sample(Sampler, IN.TextureCoordinate).r;
    float metalness = CustomRoughnessMetalnessSkipIndSpec.g >= 0.0f ? CustomRoughnessMetalnessSkipIndSpec.g : MetallicMap.Sample(Sampler, IN.TextureCoordinate).r;
    float reflectionMask = CustomRoughnessMetalnessSkipIndSpec.b > 0.0 ? 0.5f : ReflectionMaskMap.Sample(Sampler, IN.TextureCoordinate).r;
    OUT.Extra = float4(reflectionMask, roughness, metalness, Reflection_Foliage_UseGlobalDiffuseProbe_POM_MaskFactor.g);
    OUT.Extra2 = float4(
        Reflection_Foliage_UseGlobalDiffuseProbe_POM_MaskFactor.b, 
        Reflection_Foliage_UseGlobalDiffuseProbe_POM_MaskFactor.a ? HeightMap.Sample(Sampler, IN.TextureCoordinate).r : -1.0f, 
        SkipDeferredLighting_UseSSS_CustomAlphaDiscard.g,
        SkipDeferredLighting_UseSSS_CustomAlphaDiscard.r);
    return OUT;
}