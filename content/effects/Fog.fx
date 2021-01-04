Texture2D ColorTexture;
Texture2D DepthTexture;

cbuffer CBufferPerObject
{
    float4 FogColor;
    float FogNear;
    float FogFar;
    float FogDensity;
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

float linearizeDepth(float depth, float NearPlaneZ, float FarPlaneZ)
{
    float ProjectionA = FarPlaneZ / (FarPlaneZ - NearPlaneZ);
    float ProjectionB = (-FarPlaneZ * NearPlaneZ) / (FarPlaneZ - NearPlaneZ);

	// Sample the depth and convert to linear view space Z (assume it gets sampled as
	// a floating point value of the range [0,1])
    float linearDepth = ProjectionB / (depth - ProjectionA);
    return linearDepth;
}


/************* Pixel Shaders *************/

float4 fog_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    float2 texCoord = IN.TextureCoordinate;
    float4 color = ColorTexture.Sample(TrilinearSampler, IN.TextureCoordinate);
    float depth = DepthTexture.Sample(TrilinearSampler, texCoord).r;
    
    if (depth > 0.9999f)
        return color;
    
    depth = linearizeDepth(depth, FogNear, FogFar);
    
    //float density = saturate(depth * FogDensity);
    float density = depth / FogDensity;
    return lerp(color, FogColor, density);
}


float4 no_filter_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    return ColorTexture.Sample(TrilinearSampler, IN.TextureCoordinate);

}

/************* Techniques *************/

technique11 fog
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, fog_pixel_shader()));
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