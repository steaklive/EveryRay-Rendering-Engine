static const int TILE_SIZE = 512;
static const int DETAIL_TEXTURE_REPEAT = 32;
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
    float4 CameraPosition;
}
cbuffer CBufferTerrainData
{
    float TessellationFactor;
    float TerrainHeightScale;
    float UseDynamicTessellation;
    float TessellationFactorDynamic;
    float DistanceFactor;
};
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
    centroid float2 texcoord : TEXCOORD0;
    centroid float3 normal : NORMAL;
};

struct PatchData
{
    float Edges[4] : SV_TessFactor;
    float Inside[2] : SV_InsideTessFactor;

    float2 origin : ORIGIN;
    float2 size : SIZE;
};

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
    AddressW = WRAP;
    //MaxAnisotropy = 16;
};

SamplerState TerrainHeightSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = CLAMP;//WRAP;
    AddressV = CLAMP;//WRAP;
    AddressW = CLAMP;//WRAP;
    //MaxAnisotropy = 16;
};

SamplerState BilinearSampler
{
    Filter = MIN_MAG_LINEAR_MIP_POINT;
    AddressU = CLAMP;
    AddressV = CLAMP;
};
Texture2D heightTexture;
Texture2D splatTexture;
Texture2D groundTexture;
Texture2D grassTexture;
Texture2D rockTexture;
Texture2D mudTexture;


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

// hyperbolic function that determines a factor
float GetTessellationFactorFromCamera(float distance)
{
    return lerp(TessellationFactor, TessellationFactorDynamic * (1 / (DistanceFactor * distance)), UseDynamicTessellation);
}

// https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/chapter5-andersson-terrain-rendering-in-frostbite.pdf
float3 GetNormalFromHeightmap(float2 uv, float texelSize, float maxHeight)
{
    float4 h;
    h[0] = heightTexture.SampleLevel(BilinearSampler, uv + texelSize * float2(0, -1), 0).r * maxHeight;
    h[1] = heightTexture.SampleLevel(BilinearSampler, uv + texelSize * float2(-1, 0), 0).r * maxHeight;
    h[2] = heightTexture.SampleLevel(BilinearSampler, uv + texelSize * float2(1, 0), 0).r * maxHeight;
    h[3] = heightTexture.SampleLevel(BilinearSampler, uv + texelSize * float2(0, 1), 0).r * maxHeight;
    
    float3 n;
    n.z = h[0] - h[3];
    n.x = h[1] - h[2];
    n.y = 2;

    return normalize(n);
}

PatchData hull_constant_function(InputPatch<HS_INPUT, 1> inputPatch)
{
    PatchData output;

    float distance_to_camera = 0.0f;
    float tesselation_factor = 0.0f;
    float inside_tessellation_factor = 0.0f;
    //float in_frustum = 0;

    output.origin = inputPatch[0].origin;
    output.size = inputPatch[0].size;
    
    float4 pos = float4(inputPatch[0].origin.x, 0.0f, inputPatch[0].origin.y, 1.0f);
    pos = mul(pos, World);

    distance_to_camera = length(CameraPosition.xz - pos.xz - float2(0, inputPatch[0].size.y * 0.5));
    tesselation_factor = GetTessellationFactorFromCamera(distance_to_camera);
    output.Edges[0] = tesselation_factor;
    inside_tessellation_factor += tesselation_factor;

    distance_to_camera = length(CameraPosition.xz - pos.xz - float2(inputPatch[0].size.x * 0.5, 0));
    tesselation_factor = GetTessellationFactorFromCamera(distance_to_camera);
    output.Edges[1] = tesselation_factor;
    inside_tessellation_factor += tesselation_factor;

    distance_to_camera = length(CameraPosition.xz - pos.xz - float2(inputPatch[0].size.x, inputPatch[0].size.y * 0.5));
    tesselation_factor = GetTessellationFactorFromCamera(distance_to_camera);
    output.Edges[2] = tesselation_factor;
    inside_tessellation_factor += tesselation_factor;

    distance_to_camera = length(CameraPosition.xz - pos.xz - float2(inputPatch[0].size.x * 0.5, inputPatch[0].size.y));
    tesselation_factor = GetTessellationFactorFromCamera(distance_to_camera);
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
    
    float2 texcoord01 = (input.origin + uv * input.size) / float(TILE_SIZE);
    float height = heightTexture.SampleLevel(TerrainHeightSampler, texcoord01, 0).r;
	
    vertexPosition.xz = input.origin + uv * input.size;
    vertexPosition.y = TerrainHeightScale * height;
   
    //calculating base normal rotation matrix
    //float3 normal = normalize(/*2.0f **/ normalTexture.SampleLevel(TerrainSplatSampler, float2(texcoord01.x, 1.0f - texcoord01.y), 0).rgb /*- float3(1.0f, 1.0f, 1.0f)*/);
    //normal.z = -normal.z;
    //float3x3 normal_rotation_matrix;
    //normal_rotation_matrix[1] = normal;
    //normal_rotation_matrix[2] = normalize(cross(float3(-1.0, 0.0, 0.0), normal_rotation_matrix[1]));
    //normal_rotation_matrix[0] = normalize(cross(normal_rotation_matrix[2], normal_rotation_matrix[1]));

	//applying base rotation matrix to detail normal
    //float3 normalRot = mul(normal, normal_rotation_matrix);
    
	// writing output params
    output.position = mul(float4(vertexPosition, 1.0), World);
    output.position = mul(output.position, View);
    output.position = mul(output.position, Projection);
    output.texcoord = texcoord01;
    output.normal = float3(0, 0, 0);
    return output;
}


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

float4 pixel_shader_ts(DS_OUTPUT IN) : SV_Target
{   
    float2 uvTile = IN.texcoord;
    
    //float4 height = heightTexture.Sample(TerrainHeightSampler, uvTile);
    float3 normal = GetNormalFromHeightmap(uvTile, 1.0f / (float) (TILE_SIZE), TerrainHeightScale);
   
    uvTile.y = 1.0f - uvTile.y;
    
    float4 splat = splatTexture.Sample(TerrainSplatSampler, uvTile);
    float3 ground = groundTexture.Sample(TerrainTextureSampler, uvTile * DETAIL_TEXTURE_REPEAT).rgb;
    float3 grass = grassTexture.Sample(TerrainTextureSampler, uvTile * DETAIL_TEXTURE_REPEAT).rgb;
    float3 rock = rockTexture.Sample(TerrainTextureSampler, uvTile * DETAIL_TEXTURE_REPEAT).rgb;
    float3 mud = mudTexture.Sample(TerrainTextureSampler, uvTile * DETAIL_TEXTURE_REPEAT).rgb;
    
    float3 albedo = splat.r * mud + splat.g * grass + splat.b * rock + splat.a * ground;
    
    float3 color = AmbientColor.rgb;

    float lightIntensity = saturate(dot(normal, SunDirection.rgb));

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

    pass tessellation
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader_ts()));
        SetHullShader(CompileShader(hs_5_0, hull_shader()));
        SetDomainShader(CompileShader(ds_5_0, domain_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, pixel_shader_ts()));
    }
}