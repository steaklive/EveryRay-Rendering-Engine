cbuffer CBufferPerObject
{
    float4x4 ViewProjection;
    float4x4 World;
    float4 CameraPosition;
}
TextureCubeArray<float4> CubemapTexture;

SamplerState LinearSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
    AddressW = Wrap;
};


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


VS_OUTPUT mainVS(VS_INPUT IN)
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

VS_OUTPUT mainVS_Instancing(VS_INPUT_INSTANCING IN)
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

float4 mainPS(VS_OUTPUT vsOutput) : SV_Target0
{
    float3 viewDir = normalize(CameraPosition.xyz - vsOutput.WorldPos);
    float3 reflectDir = normalize(reflect(-viewDir, vsOutput.Normal));
   
    if (vsOutput.CullingFlag > 0.0f) 
        discard;//    return float4(0.5f, 0.5f, 0.5f, 1.0f);
    //else
        return float4(CubemapTexture.Sample(LinearSampler, float4(reflectDir, vsOutput.CubemapIndex)).rgb, 1.0f);
}

float3 recomputePS(VS_OUTPUT vsOutput) : SV_Target0
{
    float3 color = float3(1.0f, 0.0f, 0.0f);
    return color;
}

/************* Techniques *************/

technique11 main
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, mainVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, mainPS()));
    }
}

technique11 main_instancing
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, mainVS_Instancing()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, mainPS()));
    }
}

technique11 recompute
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, mainVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, recomputePS()));
    }
}

technique11 recompute_instancing
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, mainVS_Instancing()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, recomputePS()));
    }
}