Texture2D AlbedoMap;
Texture2D RoughnessMap;

cbuffer CBufferPerObject
{
    float4x4 ViewProjection;
    float4x4 World : WORLD;
    float ReflectionMaskFactor;
}
SamplerState Sampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = WRAP;
    AddressV = WRAP;
};

/************* Data Structures *************/

struct VS_INPUT
{
    float4 ObjectPosition : POSITION;
    float2 TextureCoordinate : TEXCOORD;
    float3 Normal : NORMAL;
};
    
struct VS_INPUT_INSTANCING
{
    float4 ObjectPosition : POSITION;
    float2 TextureCoordinate : TEXCOORD;
    float3 Normal : NORMAL;
    
    //instancing
    row_major float4x4 World : WORLD;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL;
    float2 TextureCoordinate : TEXCOORD0;
    float4 WorldPos : TEXCOORD1;
};

VS_OUTPUT vertex_shader(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;
    
    OUT.Position = mul(IN.ObjectPosition, mul(World, ViewProjection));
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), World).xyz);
    OUT.TextureCoordinate = IN.TextureCoordinate;
    OUT.WorldPos = mul(IN.ObjectPosition, World);
    
    return OUT;
}
VS_OUTPUT vertex_shader_instancing(VS_INPUT_INSTANCING IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;
    
    OUT.WorldPos = mul(IN.ObjectPosition, IN.World);
    OUT.Position = mul(OUT.WorldPos, ViewProjection);
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), IN.World).xyz);
    OUT.TextureCoordinate = IN.TextureCoordinate;
    
    return OUT;
}

struct PS_OUTPUT
{
    float4 Color : SV_Target0;
    float4 Normal : SV_Target1;
    float4 WorldPos : SV_Target2;
    float4 Extra : SV_Target3;
};

PS_OUTPUT pixel_shader(VS_OUTPUT IN) : SV_Target
{
    PS_OUTPUT OUT;

    OUT.Color = AlbedoMap.Sample(Sampler, IN.TextureCoordinate);
    OUT.Normal = float4(IN.Normal, 1.0f);
    OUT.WorldPos = IN.WorldPos;
    float roughnessMask = RoughnessMap.Sample(Sampler, IN.TextureCoordinate).r;
    OUT.Extra = float4(ReflectionMaskFactor, roughnessMask, 0.0, 0.0);
    return OUT;

}
technique10 deferred
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetPixelShader(CompileShader(ps_5_0, pixel_shader()));
    }
}
technique10 deferred_instanced
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader_instancing()));
        SetPixelShader(CompileShader(ps_5_0, pixel_shader()));
    }
}