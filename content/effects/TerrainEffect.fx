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
struct VS_INPUT
{
	float4 ObjectPosition: POSITION;
    float4 TextureCoordinates : TEXCOORD;
	float3 Normal : NORMAL;
};
struct VS_OUTPUT 
{
	float4 Position: SV_Position;
	float4 TextureCoordinates : TEXCOORD0;	
    float3 Normal : NORMAL;
};

VS_OUTPUT vertex_shader(VS_INPUT IN)
{
	VS_OUTPUT OUT = (VS_OUTPUT)0;
	
    OUT.Position = mul(IN.ObjectPosition, World);
    OUT.Position = mul(OUT.Position, View);
    OUT.Position = mul(OUT.Position, Projection);
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), World).xyz);
    OUT.TextureCoordinates = IN.TextureCoordinates;
	return OUT;
}
SamplerState TerrainTextureSampler
{
    Filter = Anisotropic;
    AddressU = WRAP;
    AddressV = WRAP;
};

SamplerState TerrainSplatSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = WRAP;
    AddressV = WRAP;
};

Texture2D splatTexture;
Texture2D groundTexture;
Texture2D grassTexture;
Texture2D rockTexture;
Texture2D mudTexture;

float4 pixel_shader(VS_OUTPUT IN) : SV_Target
{
    float4 splat = splatTexture.Sample(TerrainSplatSampler, IN.TextureCoordinates.zw);
    
    float3 ground = groundTexture.Sample(TerrainTextureSampler, IN.TextureCoordinates.xy).rgb;
    float3 grass = grassTexture.Sample(TerrainTextureSampler, IN.TextureCoordinates.xy).rgb;
    float3 rock = rockTexture.Sample(TerrainTextureSampler, IN.TextureCoordinates.xy).rgb;
    float3 mud = mudTexture.Sample(TerrainTextureSampler, IN.TextureCoordinates.xy).rgb;
    
    float3 albedo = splat.r * mud + splat.g * grass + splat.b * rock + splat.a * ground;
        
    float3 color = AmbientColor.rgb;

    float lightIntensity = saturate(dot(IN.Normal, SunDirection.rgb));

    if (lightIntensity > 0.0f)
        color += SunColor.rgb * lightIntensity;

    color = saturate(color);
    color *= albedo;
    
    return float4(color, 1.0f);
}
technique11 main
{
    pass p0
	{
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
		SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, pixel_shader()));
    }
}