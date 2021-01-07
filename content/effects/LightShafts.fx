Texture2D ColorTexture;
Texture2D DepthTexture;
Texture2D OcclusionTexture;

SamplerState DefaultSampler;

cbuffer Params
{
    float4 SunPosNDC;
    float Density;
    float Weight;
    float Exposure;
    float Decay;
    float MaxSunDistanceDelta;
    float Intensity;
};

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
float3 toClipSpaceCoord(float2 tex)
{
    float2 ray;
    ray.x = 2.0 * tex.x - 1.0;
    ray.y = 1.0 - tex.y * 2.0;
    
    return float3(ray, 1.0);
}

/************* Pixel Shaders *************/

float4 light_shafts_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    const int Samples = 100;
    const float Delta = 1.0f / (float) Samples;
    
    float4 color;
    float2 tex = IN.TextureCoordinate;
    
    float2 deltaTextCoord = IN.TextureCoordinate - SunPosNDC.xy;
    float2 distanceToSun = (deltaTextCoord);
    deltaTextCoord = min(MaxSunDistanceDelta, distanceToSun * Delta);
    deltaTextCoord *= Density;
    	
    float illuminationDecay = 1.0;

	// Evaluate the summation of shadows from occlusion texture
    for (int i = 0; i < Samples; i++)
    {
        tex -= deltaTextCoord;
        float4 colorSample = OcclusionTexture.Sample(DefaultSampler, clamp(tex, 0, 1));
        colorSample *= illuminationDecay * Weight;
        color += colorSample;
        illuminationDecay *= Decay;
    }

    color *= Exposure;

    float4 sceneColor = ColorTexture.Sample(DefaultSampler, IN.TextureCoordinate);
    return lerp(sceneColor, sceneColor + color, Intensity);
}

float4 no_filter_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    return ColorTexture.Sample(DefaultSampler, IN.TextureCoordinate);

}

/************* Techniques *************/
technique11 light_shafts
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, light_shafts_pixel_shader()));
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