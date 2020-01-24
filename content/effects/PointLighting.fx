
#define FLIP_TEXTURE_Y 0

cbuffer CBufferPerFrame
{
	float4 AmbientColor : AMBIENT <
		string UIName =  "Ambient Light";
		string UIWidget = "Color";
	> = {1.0f, 1.0f, 1.0f, 0.0f};

	float4 LightColor : COLOR <
		string Object = "LightColor0";
		string UIName =  "Light Color";
		string UIWidget = "Color";
	> = {1.0f, 1.0f, 1.0f, 1.0f};

	float3 LightPosition : POSITION <
		string Object = "PointLight0";
		string UIName =  "Light Position";
		string Space = "World";
	> = {0.0f, 0.0f, 0.0f};

	float LightRadius <
		string UIName =  "Light Radius";
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 100.0;
		float UIStep = 1.0;
	> = {10.0f};
	
	float3 CameraPosition : CAMERAPOSITION < string UIWidget="None"; >;
}

cbuffer CBufferPerObject
{
	float4x4 WorldViewProjection : WORLDVIEWPROJECTION < string UIWidget="None"; >;
	float4x4 World : WORLD < string UIWidget="None"; >;
	
	float4 SpecularColor : SPECULAR <
    	string UIName =  "Specular Color";
    	string UIWidget = "Color";
	> = {1.0f, 1.0f, 1.0f, 1.0f};

	float SpecularPower : SPECULARPOWER <
		string UIName =  "Specular Power";
		string UIWidget = "slider";
		float UIMin = 1.0;
		float UIMax = 255.0;
		float UIStep = 1.0;
	> = {25.0f};
}

Texture2D ColorTexture <
    string ResourceName = "default_color.dds";
    string UIName =  "Color Texture";
    string ResourceType = "2D";
>;

SamplerState ColorSampler
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = WRAP;
	AddressV = WRAP;
};

/************* Data Structures *************/

struct VS_INPUT
{
    float4 ObjectPosition : POSITION;
    float2 TextureCoordinate : TEXCOORD;
	float3 Normal : NORMAL;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL;
	float2 TextureCoordinate : TEXCOORD0;	
	float3 WorldPosition : TEXCOORD1;
	float Attenuation : TEXCOORD2;
};

float2 get_corrected_texture_coordinate(float2 textureCoordinate)
{
#if FLIP_TEXTURE_Y
	return float2(textureCoordinate.x, 1.0 - textureCoordinate.y);
#else
	return textureCoordinate;
#endif
}


float3 get_vector_color_contribution(float4 light, float3 color)
{
	// Color (.rgb) * Intensity (.a)
	return light.rgb * light.a * color;
}

float3 get_scalar_color_contribution(float4 light, float color)
{
	// Color (.rgb) * Intensity (.a)
	return light.rgb * light.a * color;
}


/************* Vertex Shader *************/
VS_OUTPUT vertex_shader(VS_INPUT IN)
{
	VS_OUTPUT OUT = (VS_OUTPUT)0;
	
    OUT.Position = mul(IN.ObjectPosition, WorldViewProjection);
    OUT.WorldPosition = mul(IN.ObjectPosition, World).xyz;
	OUT.TextureCoordinate = get_corrected_texture_coordinate(IN.TextureCoordinate);
	OUT.Normal = normalize(mul(float4(IN.Normal, 0), World).xyz);	

	float3 lightDirection = LightPosition - OUT.WorldPosition;
	OUT.Attenuation = saturate(1.0f - (length(lightDirection) / LightRadius)); // Attenuation	
	
	return OUT;
}

/************* Pixel Shader *************/

float4 pixel_shader(VS_OUTPUT IN) : SV_Target
{
	float4 OUT = (float4)0;
	
	float3 lightDirection = LightPosition - IN.WorldPosition;
    lightDirection = normalize(lightDirection);

	float3 viewDirection = normalize(CameraPosition - IN.WorldPosition);
	
	float3 normal = normalize(IN.Normal);	
	float n_dot_l = dot(normal, lightDirection);	
	float3 halfVector = normalize(lightDirection + viewDirection);
	float n_dot_h = dot(normal, halfVector);

	float4 color = ColorTexture.Sample(ColorSampler, IN.TextureCoordinate);
	float4 lightCoefficients = lit(n_dot_l, n_dot_h, SpecularPower);

	float3 ambient = get_vector_color_contribution(AmbientColor, color.rgb);
	float3 diffuse = get_vector_color_contribution(LightColor,  lightCoefficients.y * color.rgb) * IN.Attenuation;
	float3 specular = get_scalar_color_contribution(SpecularColor, min(lightCoefficients.z, color.w)) * IN.Attenuation;

	OUT.rgb = ambient + diffuse + specular;
	OUT.a = 1.0f;
	
	return OUT;
}

/************* Techniques *************/

technique11 main11
{
    pass p0
	{
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
		SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, pixel_shader()));
    }
}