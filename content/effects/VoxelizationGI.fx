cbuffer VoxelizationCB : register(b0)
{
    float4x4 ViewProjection;
    float4x4 LightViewProjection;
    float WorldVoxelScale;
};

cbuffer ModelCB : register(b1)
{
    float4x4 MeshWorld;
};

RWTexture3D<float4> outputTexture : register(u0);

Texture2D<float> MeshAlbedo : register(t0);
Texture2D<float> ShadowMap : register(t1);
//SamplerComparisonState PcfShadowMapSampler : register(s0);

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
};

struct PS_IN
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float3 VoxelPos : TEXCOORD1;
};

GS_IN VSMain(VS_IN input)
{
    GS_IN output = (GS_IN) 0;
    
    output.Position = mul(float4(input.Position.xyz, 1), MeshWorld);
    output.UV = input.UV;
    return output;
}

GS_IN VSMain_Instancing(VS_IN_INSTANCING input)
{
    GS_IN output = (GS_IN) 0;
    
    output.Position = mul(float4(input.Position.xyz, 1), input.World);
    output.UV = input.UV;
    return output;
}

[maxvertexcount(3)]
void GSMain(triangle GS_IN input[3], inout TriangleStream<PS_IN> OutputStream)
{
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
        output[0].VoxelPos = input[i].Position.xyz / WorldVoxelScale * 2.0f;
        output[1].VoxelPos = input[i].Position.xyz / WorldVoxelScale * 2.0f;
        output[2].VoxelPos = input[i].Position.xyz / WorldVoxelScale * 2.0f;
        if (axis == n.z)
            output[i].Position = float4(output[i].VoxelPos.x, output[i].VoxelPos.y, 0, 1);
        else if (axis == n.x)
            output[i].Position = float4(output[i].VoxelPos.y, output[i].VoxelPos.z, 0, 1);
        else
            output[i].Position = float4(output[i].VoxelPos.x, output[i].VoxelPos.z, 0, 1);
    
        //output[i].normal = input[i].normal;
        output[i].UV = input[i].UV;
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
    
    float4 worldPos = float4(VoxelToWorld(voxelPos), 1.0f);
    //float4 lightSpacePos = mul(ShadowViewProjection, worldPos);
    //float4 shadowcoord = lightSpacePos / lightSpacePos.w;
    //shadowcoord.rg = shadowcoord.rg * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    //float shadow = shadowBuffer.SampleCmpLevelZero(PcfShadowMapSampler, shadowcoord.xy, shadowcoord.z);

    outputTexture[finalVoxelPos] = colorRes;// * float4(shadow, shadow, shadow, 1.0f);
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