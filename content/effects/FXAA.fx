
Texture2D ColorTexture;
cbuffer CBufferPerObject
{
    float Width;
    float Height;
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

float4 fxaa(VS_OUTPUT IN) : SV_Target
{
    float2 texCoord = IN.TextureCoordinate;
    float2 resolution = float2(Width, Height);

    float FXAA_SPAN_MAX = 8.0;
    float FXAA_REDUCE_MUL = 1.0 / 8.0;
    float FXAA_REDUCE_MIN = 1.0 / 128.0;
    
    float3 rgbNW = ColorTexture.Sample(TrilinearSampler, texCoord + float2(-1.0, -1.0) / resolution, 0).rgb;
    float3 rgbNE = ColorTexture.Sample(TrilinearSampler, texCoord + float2(1.0, -1.0) / resolution, 0).rgb;
    float3 rgbSW = ColorTexture.Sample(TrilinearSampler, texCoord + float2(-1.0, 1.0) / resolution, 0).rgb;
    float3 rgbSE = ColorTexture.Sample(TrilinearSampler, texCoord + float2(1.0, 1.0) / resolution, 0).rgb;
    float3 rgbM =  ColorTexture.Sample(TrilinearSampler, texCoord, 0).rgb;
    
    float3 luma = float3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM = dot(rgbM, luma);
    
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    
    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    
    float dirReduce = max(
        (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
        FXAA_REDUCE_MIN);
    
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    
    dir = min(float2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
          max(float2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
          dir * rcpDirMin)) / resolution;
    
    float3 rgbA = (1.0 / 2.0) * 
    (
        ColorTexture.Sample(TrilinearSampler, texCoord + dir * (1.0 / 3.0 - 0.5), 0).rgb +
        ColorTexture.Sample(TrilinearSampler, texCoord + dir * (2.0 / 3.0 - 0.5), 0).rgb
    );
    
    float3 rgbB = rgbA * (1.0 / 2.0) + (1.0 / 4.0) *
    (
        ColorTexture.Sample(TrilinearSampler, texCoord + dir * (0.0 / 3.0 - 0.5), 0).rgb +
        ColorTexture.Sample(TrilinearSampler, texCoord + dir * (3.0 / 3.0 - 0.5), 0).rgb
    );
    
    float lumaB = dot(rgbB, luma);

    float3 finalColor;
    if ((lumaB < lumaMin) || (lumaB > lumaMax))
    {
        finalColor.xyz = rgbA;
    }
    else
    {
        finalColor.xyz = rgbB;
    }
    return float4(finalColor, 1.0f);
	
}


float4 no_filter_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    return ColorTexture.Sample(TrilinearSampler, IN.TextureCoordinate);

}

/************* Techniques *************/

technique11 fxaa_filter
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, fxaa()));
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