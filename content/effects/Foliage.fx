cbuffer CBufferPerObject
{
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
}

cbuffer CBufferPerFrame
{
    float4 SunDirection;
    float4 SunColor;
    float4 AmbientColor;
    float4 ShadowTexelSize;
    float4 ShadowCascadeDistances;
}

SamplerState ColorSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = WRAP;
    AddressV = WRAP;
    AddressW = WRAP;
};

Texture2D albedoTexture;

struct VS_INPUT
{
    float4 Position : POSITION;
    float2 TextureCoordinates : TEXCOORD0;
    row_major float4x4 World : WORLD;
    float3 Color : TEXCOORD1;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float2 TextureCoordinates : TEXCOORD0;
    float3 Color : TEXCOORD1;
};

VS_OUTPUT vertex_shader(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;

    OUT.Position = mul(IN.Position, IN.World);
    OUT.Position = mul(OUT.Position, View);
    OUT.Position = mul(OUT.Position, Projection);
    OUT.TextureCoordinates = IN.TextureCoordinates;
    OUT.Color = IN.Color;
    
    return OUT;
}
float4 pixel_shader(VS_OUTPUT IN) : SV_Target
{
    float4 albedo = albedoTexture.Sample(ColorSampler, IN.TextureCoordinates);

    float4 color = albedo * SunColor * float4(0.8f, 0.8f, 0.8, 1.0f);
    
    // Saturate the final color result.
    color = saturate(color);

    return color;
}

/************* Techniques *************/
technique11 main
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, pixel_shader()));

    }
}