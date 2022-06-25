#include "Common.hlsli"

cbuffer FoliageCBuffer : register(b0)
{
    float4x4 ShadowMatrices[NUM_OF_SHADOW_CASCADES];
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
    float4 SunDirection;
    float4 SunColor;
    float4 AmbientColor;
    float4 ShadowTexelSize;
    float4 ShadowCascadeDistances;
    float4 CameraDirection;
    float4 CameraPos;
    float4 VoxelCameraPos;
    float4 WindDirection;
    float RotateToCamera;
    float Time;
    float WindFrequency;
    float WindStrength;
    float WindGustDistance;
    float WorldVoxelScale;
    float VoxelTextureDimension;
}

SamplerState ColorSampler : register(s0);
SamplerComparisonState CascadedPcfShadowMapSampler : register(s1);

Texture2D<float4> AlbedoTexture : register(t0);
Texture2D<float> CascadedShadowTextures[NUM_OF_SHADOW_CASCADES] : register(t1);

RWTexture3D<float4> OutputVoxelGITexture : register(u0);

struct VS_INPUT
{
    float4 Position : POSITION;
    float2 TextureCoordinates : TEXCOORD0;
    float3 Normal : NORMAL;
    
    row_major float4x4 World : WORLD;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float2 TextureCoordinates : TEXCOORD0;
    float3 Normal : Normal;
    float3 ShadowCoord0 : TEXCOORD2;
    float3 ShadowCoord1 : TEXCOORD3;
    float3 ShadowCoord2 : TEXCOORD4;
    float3 WorldPos : WorldPos;
};

struct PS_GI_IN
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float3 VoxelPos : TEXCOORD1;
    float3 ShadowCoord : TEXCOORD2;
    float4 PosWVP : TEXCOORD3;
};
struct PS_OUTPUT_GBUFFER
{
    float4 Color : SV_Target0;
    float4 Normal : SV_Target1;
    float4 WorldPos : SV_Target2;
    float4 Extra : SV_Target3;
    float4 Extra2 : SV_Target4;
};

VS_OUTPUT VSMain(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;
    
    float4x4 scaleMat;
    float scaleX = sqrt(IN.World[0][0] * IN.World[0][0] + IN.World[0][1] * IN.World[0][1] + IN.World[0][2] * IN.World[0][2]);
    float scaleY = sqrt(IN.World[1][0] * IN.World[1][0] + IN.World[1][1] * IN.World[1][1] + IN.World[1][2] * IN.World[1][2]);
    float scaleZ = sqrt(IN.World[2][0] * IN.World[2][0] + IN.World[2][1] * IN.World[2][1] + IN.World[2][2] * IN.World[2][2]);
    
    scaleMat[0][0] = scaleX;
    scaleMat[0][1] = 0.0f;
    scaleMat[0][2] = 0.0f;
    scaleMat[0][3] = 0.0f;
    scaleMat[1][0] = 0.0f;
    scaleMat[1][1] = scaleY;
    scaleMat[1][2] = 0.0f;
    scaleMat[1][3] = 0.0f;
    scaleMat[2][0] = 0.0f;
    scaleMat[2][1] = 0.0f;
    scaleMat[2][2] = scaleZ;
    scaleMat[2][3] = 0.0f;
    scaleMat[3][0] = 0.0f;
    scaleMat[3][1] = 0.0f;
    scaleMat[3][2] = 0.0f;
    scaleMat[3][3] = 1.0f;
    
    float4x4 translateMat;
    translateMat[0][0] = 1.0f;
    translateMat[0][1] = 0.0f;
    translateMat[0][2] = 0.0f;
    translateMat[0][3] = 0.0f;
    translateMat[1][0] = 0.0f;
    translateMat[1][1] = 1.0f;
    translateMat[1][2] = 0.0f;
    translateMat[1][3] = 0.0f;
    translateMat[2][0] = 0.0f;
    translateMat[2][1] = 0.0f;
    translateMat[2][2] = 1.0f;
    translateMat[2][3] = 0.0f;
    translateMat[3][0] = IN.World[3][0];
    translateMat[3][1] = IN.World[3][1];
    translateMat[3][2] = IN.World[3][2];
    translateMat[3][3] = IN.World[3][3];
    
    float4x4 rotationMat;
    rotationMat[0][0] = IN.World[0][0] / scaleX;
    rotationMat[0][1] = IN.World[0][1] / scaleX;
    rotationMat[0][2] = IN.World[0][2] / scaleX;
    rotationMat[0][3] = 0.0f;
    rotationMat[1][0] = IN.World[1][0] / scaleY;
    rotationMat[1][1] = IN.World[1][1] / scaleY;
    rotationMat[1][2] = IN.World[1][2] / scaleY;
    rotationMat[1][3] = 0.0f;
    rotationMat[2][0] = IN.World[2][0] / scaleZ;
    rotationMat[2][1] = IN.World[2][1] / scaleZ;
    rotationMat[2][2] = IN.World[2][2] / scaleZ;
    rotationMat[2][3] = 0.0f;
    rotationMat[3][0] = 0.0f;
    rotationMat[3][1] = 0.0f;
    rotationMat[3][2] = 0.0f;
    rotationMat[3][3] = 1.0f;
    
    float4 localPos = IN.Position;
    float vertexHeight = 0.5f;
    
    float angle = atan2(CameraDirection.x, CameraDirection.z) * (180.0 / PI);
    angle *= 0.0174532925f;
    
    float4x4 newRotationMat;
    newRotationMat[0][0] = cos(angle);
    newRotationMat[0][1] = 0.0f;
    newRotationMat[0][2] = -sin(angle);
    newRotationMat[0][3] = 0.0f;
    newRotationMat[1][0] = 0.0f;
    newRotationMat[1][1] = 1.0f;
    newRotationMat[1][2] = 0.0f;
    newRotationMat[1][3] = 0.0f;
    newRotationMat[2][0] = sin(angle);
    newRotationMat[2][1] = 0.0f;
    newRotationMat[2][2] = cos(angle);
    newRotationMat[2][3] = 0.0f;
    newRotationMat[3][0] = 0.0f;
    newRotationMat[3][1] = 0.0f;
    newRotationMat[3][2] = 0.0f;
    newRotationMat[3][3] = 1.0f;
    
    localPos = mul(localPos, scaleMat);
    if (RotateToCamera > 0.0f)
        localPos = mul(localPos, newRotationMat);
    localPos = mul(localPos, translateMat);
    OUT.Position = localPos;
    {
        
        //OUT.Position = mul(localPos, IN.World);
        //OUT.Position = mul(OUT.Position, rotationMat);
        if (IN.Position.y > vertexHeight)
        {
            OUT.Position.x += sin(Time * WindFrequency + OUT.Position.x * WindGustDistance) * vertexHeight * WindStrength * WindDirection.x;
            OUT.Position.z += sin(Time * WindFrequency + OUT.Position.z * WindGustDistance) * vertexHeight * WindStrength * WindDirection.z;
        }
        OUT.WorldPos = OUT.Position;
        OUT.Position = mul(OUT.Position, View);
    }

    OUT.Position = mul(OUT.Position, Projection);
    IN.Normal = float3(0.0, 1.0, 0.0);
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), IN.World).xyz);
    OUT.TextureCoordinates = IN.TextureCoordinates;
    OUT.ShadowCoord0 = mul(IN.Position, mul(IN.World, ShadowMatrices[0])).xyz;
    OUT.ShadowCoord1 = mul(IN.Position, mul(IN.World, ShadowMatrices[1])).xyz;
    OUT.ShadowCoord2 = mul(IN.Position, mul(IN.World, ShadowMatrices[2])).xyz;
    
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
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy, ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d2, d1), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d1, -d2), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d2, -d1), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d1, d2), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d4, d3), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d3, -d4), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d4, -d3), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d3, d4), ShadowCoord.z)
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

[maxvertexcount(3)]
void GSMain(triangle VS_OUTPUT input[3], inout TriangleStream<PS_GI_IN> OutputStream)
{
    PS_GI_IN output[3];
    output[0] = (PS_GI_IN) 0;
    output[1] = (PS_GI_IN) 0;
    output[2] = (PS_GI_IN) 0;
    
    float3 p1 = input[1].WorldPos.rgb - input[0].WorldPos.rgb;
    float3 p2 = input[2].WorldPos.rgb - input[0].WorldPos.rgb;
    float3 n = abs(normalize(cross(p1, p2)));
       
    float axis = max(n.x, max(n.y, n.z));
    
    [unroll]
    for (uint i = 0; i < 3; i++)
    {
        float3 vPos = (input[i].WorldPos.xyz - VoxelCameraPos.xyz) / (0.5f * (float)VoxelTextureDimension) * WorldVoxelScale;
        output[0].VoxelPos = vPos;
        output[1].VoxelPos = vPos;
        output[2].VoxelPos = vPos;
        if (axis == n.z)
            output[i].Position = float4(output[i].VoxelPos.x, output[i].VoxelPos.y, 0, 1);
        else if (axis == n.x)
            output[i].Position = float4(output[i].VoxelPos.y, output[i].VoxelPos.z, 0, 1);
        else
            output[i].Position = float4(output[i].VoxelPos.x, output[i].VoxelPos.z, 0, 1);
    
        //output[i].normal = input[i].normal;
        output[i].UV = input[i].TextureCoordinates;
        output[i].ShadowCoord = input[i].ShadowCoord1;
        OutputStream.Append(output[i]);
    }
    OutputStream.RestartStrip();
}

float3 VoxelToWorld(float3 pos)
{
    float3 result = pos;
    result *= WorldVoxelScale;

    return result * 0.5f;
}

void PSMain_voxelization(PS_GI_IN input)
{
    uint width;
    uint height;
    uint depth;
    
    OutputVoxelGITexture.GetDimensions(width, height, depth);
    float3 voxelPos = input.VoxelPos.rgb;
    voxelPos.y = -voxelPos.y;
    
    int3 finalVoxelPos = width * float3(0.5f * voxelPos + float3(0.5f, 0.5f, 0.5f));
    float4 colorRes = AlbedoTexture.Sample(ColorSampler, input.UV);
    voxelPos.y = -voxelPos.y;
    
    float4 worldPos = float4(VoxelToWorld(voxelPos), 1.0f);
    if (ShadowCascadeDistances.a < 1.0f)
        OutputVoxelGITexture[finalVoxelPos] = colorRes;
    else
    {
        float shadow = CalculateShadow(input.ShadowCoord, 1);
        OutputVoxelGITexture[finalVoxelPos] = colorRes * SunColor * float4(shadow, shadow, shadow, 1.0f);
    }
}

float4 PSMain(VS_OUTPUT IN) : SV_Target //forward pass
{
    float4 albedo = AlbedoTexture.Sample(ColorSampler, IN.TextureCoordinates);
    float shadow = GetShadow(IN.ShadowCoord0, IN.ShadowCoord1, IN.ShadowCoord2, IN.Position.w);
    shadow = clamp(shadow, 0.5f, 1.0f);
    
    float4 color = albedo * SunColor * float4(shadow, shadow, shadow, 1.0f) * SunColor.a;
    color = saturate(color);

    return color;
}

PS_OUTPUT_GBUFFER PSMain_gbuffer(VS_OUTPUT IN) : SV_Target
{
    PS_OUTPUT_GBUFFER OUT;

    OUT.Color = AlbedoTexture.Sample(ColorSampler, IN.TextureCoordinates);
    OUT.Normal = float4(IN.Normal, 1.0f);
    OUT.WorldPos = float4(IN.WorldPos, 1.0f);
    OUT.Extra = float4(0.0, 0.0, 0.0, 1.0f);
    OUT.Extra2 = float4(0.0, -1.0f, 0.1, 0.0f); // b - custom alpha discard
    return OUT;

}
