Texture2D ColorTexture;

cbuffer CBufferPerObject
{
    float radius = 0.75;
    float softness = 0.45;
}

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

float4 vignette_filter_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    float4 color = ColorTexture.Sample(TrilinearSampler, IN.TextureCoordinate);
	
    float len = distance(IN.TextureCoordinate, float2(0.5, 0.5)) * 0.7f;
    float vignette = smoothstep(radius, radius - softness, len);

    color.rgb *= vignette;
	
   
    return color;
}


float4 no_filter_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    return ColorTexture.Sample(TrilinearSampler, IN.TextureCoordinate);

}

/************* Techniques *************/

technique11 vignette_filter
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, vignette_filter_pixel_shader()));
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