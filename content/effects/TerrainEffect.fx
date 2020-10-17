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

struct VS_INPUT_TS
{
    float4 PatchInfo : PATCH_INFO;
};

struct HS_INPUT
{
    float2 origin : ORIGIN;
    float2 size : SIZE;
};

struct HS_OUTPUT
{
    float Dummmy : DUMMY;
};

struct DS_OUTPUT
{
    float4 position : SV_Position;
};

struct PatchData
{
    float Edges[4] : SV_TessFactor;
    float Inside[2] : SV_InsideTessFactor;

    float2 origin : ORIGIN;
    float2 size : SIZE;
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

HS_INPUT vertex_shader_ts(VS_INPUT_TS IN)
{
    HS_INPUT OUT = (HS_INPUT) 0;
	
    OUT.origin = IN.PatchInfo.xy;
    OUT.size = IN.PatchInfo.zw;
    return OUT;
}

PatchData hull_constant_function(InputPatch<HS_INPUT, 1> inputPatch)
{
    PatchData output;

    //float distance_to_camera;
    float tesselation_factor = 0.0f;
    float inside_tessellation_factor = 0.0f;
    //float in_frustum = 0;

    output.origin = inputPatch[0].origin;
    output.size = inputPatch[0].size;

    
    tesselation_factor = 1;
    output.Edges[0] = tesselation_factor;
    inside_tessellation_factor += tesselation_factor;

    output.Edges[1] = tesselation_factor;
    inside_tessellation_factor += tesselation_factor;

    output.Edges[2] = tesselation_factor;
    inside_tessellation_factor += tesselation_factor;

    output.Edges[3] = tesselation_factor;
    inside_tessellation_factor += tesselation_factor;

    output.Inside[0] = output.Inside[1] = inside_tessellation_factor * 0.25;


    return output;
}

[domain("quad")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(1)]
[patchconstantfunc("hull_constant_function")]
HS_OUTPUT hull_shader(InputPatch<HS_INPUT, 1> inputPatch)
{
    return (HS_OUTPUT)0;
}

[domain("quad")]
DS_OUTPUT domain_shader(PatchData input, float2 uv : SV_DomainLocation, OutputPatch<HS_OUTPUT, 1> inputPatch)
{
    DS_OUTPUT output;
    float3 vertexPosition;
	
    vertexPosition.xz = input.origin + uv * input.size;
    vertexPosition.y = 0;//    base_texvalue.w;

	// moving vertices by detail height along base normal
    //vertexPosition += base_normal * detail_height;

	// writing output params
    output.position = mul(float4(vertexPosition, 1.0), World);
    output.position = mul(output.position, View);
    output.position = mul(output.position, Projection);
    return output;
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

float4 pixel_shader_ts(/*PS_INPUT IN*/) : SV_Target
{   
    return float4(1.0, 1.0, 1.0, 1.0);
}

technique11 main
{
    pass p0
	{
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
		SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, pixel_shader()));
    }

    pass tessellation
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader_ts()));
        SetHullShader(CompileShader(hs_5_0, hull_shader()));
        SetDomainShader(CompileShader(ds_5_0, domain_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, pixel_shader_ts()));
    }
}