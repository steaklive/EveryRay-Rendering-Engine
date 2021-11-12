#define NUM_OF_SHADOW_CASCADES 3

static const float4 colorWhite = { 1, 1, 1, 1 };
//static const float3 CascadeBBSide = { 256.0f, 256.0f, 256.0f };

cbuffer VoxelizationCB : register(b0)
{
    float4x4 ViewProjection;
    float4x4 ShadowMatrices[NUM_OF_SHADOW_CASCADES];
    float4 ShadowTexelSize;
    float4 ShadowCascadeDistances;
    float4 CameraPos;
    float WorldVoxelScale;
};

cbuffer ModelCB : register(b1)
{
    float4x4 MeshWorld;
};

RWTexture3D<float4> outputTexture : register(u0);

Texture2D MeshAlbedo : register(t0);
Texture2D ShadowMap : register(t1);

Texture2D CascadedShadowTextures[NUM_OF_SHADOW_CASCADES];

SamplerComparisonState CascadedPcfShadowMapSampler
{
    Filter = COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    AddressU = BORDER;
    AddressV = BORDER;
    BorderColor = colorWhite;
    ComparisonFunc = LESS_EQUAL;
};

SamplerState LinearSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

struct VS_IN
{
    float4 Position : POSITION;
    float2 UV : TEXCOORD;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
};

struct VS_IN_INSTANCING
{
    float4 Position : POSITION;
    float2 UV : TEXCOORD;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    
    //instancing
    row_major float4x4 World : WORLD;
};

struct GS_IN
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float3 ShadowCoord0 : TEXCOORD2;
    float3 ShadowCoord1 : TEXCOORD3;
    float3 ShadowCoord2 : TEXCOORD4;
    float4 PosWVP : TEXCOORD5;
};

struct PS_IN
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float3 VoxelPos : TEXCOORD1;
    float3 ShadowCoord0 : TEXCOORD2;
    float3 ShadowCoord1 : TEXCOORD3;
    float3 ShadowCoord2 : TEXCOORD4;
    float4 PosWVP : TEXCOORD5;
};

GS_IN VSMain(VS_IN input)
{
    GS_IN output = (GS_IN) 0;
    
    output.Position = mul(float4(input.Position.xyz, 1), MeshWorld);
    output.UV = input.UV;
    output.ShadowCoord0 = mul(input.Position, mul(MeshWorld, ShadowMatrices[0])).xyz;
    output.ShadowCoord1 = mul(input.Position, mul(MeshWorld, ShadowMatrices[1])).xyz;
    output.ShadowCoord2 = mul(input.Position, mul(MeshWorld, ShadowMatrices[2])).xyz;
    output.PosWVP = mul(float4(output.Position.rgb, 1.0f), ViewProjection);
    return output;
}

GS_IN VSMain_Instancing(VS_IN_INSTANCING input)
{
    GS_IN output = (GS_IN) 0;
    
    output.Position = mul(float4(input.Position.xyz, 1), input.World);
    output.UV = input.UV;
    output.ShadowCoord0 = mul(input.Position, mul(MeshWorld, ShadowMatrices[0])).xyz;
    output.ShadowCoord1 = mul(input.Position, mul(MeshWorld, ShadowMatrices[1])).xyz;
    output.ShadowCoord2 = mul(input.Position, mul(MeshWorld, ShadowMatrices[2])).xyz;
    output.PosWVP = mul(float4(output.Position.rgb, 1.0f), ViewProjection);
    return output;
}

[maxvertexcount(3)]
void GSMain(triangle GS_IN input[3], inout TriangleStream<PS_IN> OutputStream)
{   
    uint voxelTexWidth, voxelTexHeight, voxTexDepth;
    outputTexture.GetDimensions(voxelTexWidth, voxelTexHeight, voxTexDepth);
    
    PS_IN output[3];
    output[0] = (PS_IN) 0;
    output[1] = (PS_IN) 0;
    output[2] = (PS_IN) 0;
    
    float3 p1 = input[1].Position.rgb - input[0].Position.rgb;
    float3 p2 = input[2].Position.rgb - input[0].Position.rgb;
    float3 n = abs(normalize(cross(p1, p2)));
       
    float axis = max(n.x, max(n.y, n.z));
    
    [unroll]
    for (uint i = 0; i < 3; i++)
    {
        output[0].VoxelPos = (input[i].Position.xyz - CameraPos.xyz) / (0.5f * voxelTexWidth) * WorldVoxelScale;
        output[1].VoxelPos = (input[i].Position.xyz - CameraPos.xyz) / (0.5f * voxelTexWidth) * WorldVoxelScale;
        output[2].VoxelPos = (input[i].Position.xyz - CameraPos.xyz) / (0.5f * voxelTexWidth) * WorldVoxelScale;
        if (axis == n.z)
            output[i].Position = float4(output[i].VoxelPos.x, output[i].VoxelPos.y, 0, 1);
        else if (axis == n.x)
            output[i].Position = float4(output[i].VoxelPos.y, output[i].VoxelPos.z, 0, 1);
        else
            output[i].Position = float4(output[i].VoxelPos.x, output[i].VoxelPos.z, 0, 1);
    
        //output[i].normal = input[i].normal;
        output[i].UV = input[i].UV;
        output[i].ShadowCoord0 = input[i].ShadowCoord0;
        output[i].ShadowCoord1 = input[i].ShadowCoord1;
        output[i].ShadowCoord2 = input[i].ShadowCoord2;
        OutputStream.Append(output[i]);
    }
    OutputStream.RestartStrip();
}

//float3 VoxelToWorld(float3 pos)
//{
//    float3 result = pos;
//    result *= WorldVoxelScale;
//    return result * 0.5f;
//}

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

void PSMain(PS_IN input)
{
    uint width;
    uint height;
    uint depth;
    
    outputTexture.GetDimensions(width, height, depth);
    float3 voxelPos = input.VoxelPos.rgb;
    voxelPos.y = -voxelPos.y;
    
    int3 finalVoxelPos = width * float3(0.5f * voxelPos + float3(0.5f, 0.5f, 0.5f));
    float4 colorRes = MeshAlbedo.Sample(LinearSampler, input.UV);
    voxelPos.y = -voxelPos.y;
    
    //float4 worldPos = float4(VoxelToWorld(voxelPos), 1.0f);
    if (ShadowCascadeDistances.a < 1.0f)
        outputTexture[finalVoxelPos] = colorRes;
    else
    {
        float shadow = GetShadow(input.ShadowCoord0, input.ShadowCoord1, input.ShadowCoord2, input.PosWVP.w);
        outputTexture[finalVoxelPos] = colorRes * float4(shadow, shadow, shadow, 1.0f);
    }
}

technique11 voxelizationGI
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, VSMain()));
        SetGeometryShader(CompileShader(gs_5_0, GSMain()));
        SetPixelShader(CompileShader(ps_5_0, PSMain()));
    }
}

technique11 voxelizationGI_instancing
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, VSMain_Instancing()));
        SetGeometryShader(CompileShader(gs_5_0, GSMain()));
        SetPixelShader(CompileShader(ps_5_0, PSMain()));
    }
}