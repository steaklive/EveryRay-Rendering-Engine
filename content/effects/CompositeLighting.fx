Texture2D ColorTexture;
Texture2D InputDirectTexture;
Texture2D InputIndirectTexture;

cbuffer ConstantBuffer
{
    bool DebugVoxel;
};

SamplerState TrilinearSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = WRAP;
    AddressV = WRAP;
};

/************* Data Structures *************/

struct VS_INPUT
{
    float4 Position : POSITION;
    float2 TextureCoordinate : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float2 TextureCoordinate : TEXCOORD;
};

/************* Vertex Shader *************/

VS_OUTPUT vertex_shader(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;
    
    OUT.Position = IN.Position;
    OUT.TextureCoordinate = IN.TextureCoordinate;
    
    return OUT;
}

/************* Pixel Shaders *************/

float4 composite_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    float4 directLighting = InputDirectTexture.Sample(TrilinearSampler, IN.TextureCoordinate);
    float4 indirectLighting = InputIndirectTexture.Sample(TrilinearSampler, IN.TextureCoordinate);
    float ao = 1.0f - indirectLighting.a;

    if (!DebugVoxel)
        return saturate(directLighting + float4(indirectLighting.rgb * ao, 1.0f));
    else
        return float4(indirectLighting.rgb, 1.0f);
}


float4 no_filter_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    return InputDirectTexture.Sample(TrilinearSampler, IN.TextureCoordinate);

}

/************* Techniques *************/

technique11 composite_filter
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, composite_pixel_shader()));
    }
}

technique11 no_filter
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, no_filter_pixel_shader()));
    }
}