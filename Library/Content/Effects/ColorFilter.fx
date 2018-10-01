static const float3 GrayScaleIntensity = { 0.299f, 0.587f, 0.114f };

static const float3x3 SepiaFilter =
{
    0.393f, 0.349f, 0.272f,
    0.769f, 0.686f, 0.534f,
    0.189f, 0.168f, 0.131f
};

/************* Resources *************/

cbuffer CBufferPerObject
{
    float4x4 ColorFilter =
    {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
}

Texture2D ColorTexture;

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

float4 grayscale_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    float4 color = ColorTexture.Sample(TrilinearSampler, IN.TextureCoordinate);
    float intensity = dot(color.rgb, GrayScaleIntensity);
    
    return float4(intensity.rrr, color.a);
}

float4 inverse_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    float4 color = ColorTexture.Sample(TrilinearSampler, IN.TextureCoordinate);

    return float4(1 - color.rgb, color.a);
}

float4 sepia_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    float4 color = ColorTexture.Sample(TrilinearSampler, IN.TextureCoordinate);

    return float4(mul(color.rgb, SepiaFilter), color.a);
}

float4 genericfilter_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    float4 color = ColorTexture.Sample(TrilinearSampler, IN.TextureCoordinate);
    
    return float4(mul(color, ColorFilter).rgb, color.a);
}

/************* Techniques *************/

technique11 grayscale_filter
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, grayscale_pixel_shader()));
    }
}

technique11 inverse_filter
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, inverse_pixel_shader()));
    }
}

technique11 sepia_filter
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, sepia_pixel_shader()));
    }
}

technique11 generic_filter
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, genericfilter_pixel_shader()));
    }
}