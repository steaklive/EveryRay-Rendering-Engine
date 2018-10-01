cbuffer CBufferPerFrame
{
    float4 AmbientColor : AMBIENT <
        string UIName =  "Ambient Light";
        string UIWidget = "Color";
    > = { 1.0f, 1.0f, 1.0f, 1.0f };
    
    float4 LightColor : COLOR <
        string Object = "LightColor0";
        string UIName =  "Light Color";
        string UIWidget = "Color";
    > = { 1.0f, 1.0f, 1.0f, 1.0f };

    float3 LightDirection : DIRECTION <
        string Object = "DirectionalLight0";
        string UIName =  "Light Direction";
        string Space = "World";
    > = { 0.0f, 0.0f, -1.0f };

    float3 LightPosition : POSITION <
		string Object = "PointLight0";
		string UIName =  "Light Position";
		string Space = "World";
	> = { 0.0f, 0.0f, 0.0f };


    float LightRadius <
		string UIName =  "Light Radius";
		string UIWidget = "slider";
		float UIMin = 0.0;
		float UIMax = 100.0;
		float UIStep = 1.0;
	> = { 10.0f };

    float3 CameraPosition : CAMERAPOSITION < string UIWidget="None"; >;
}

cbuffer CBufferPerObject
{
    float4x4 WorldViewProjection : WORLDVIEWPROJECTION < string UIWidget="None"; >;
    float4x4 World : WORLD < string UIWidget="None"; >;
    
    float4 SpecularColor : SPECULAR <
        string UIName =  "Specular Color";
        string UIWidget = "Color";
    > = { 1.0f, 1.0f, 1.0f, 1.0f };

    float SpecularPower : SPECULARPOWER <
        string UIName =  "Specular Power";
        string UIWidget = "slider";
        float UIMin = 1.0;
        float UIMax = 255.0;
        float UIStep = 1.0;
    > = { 25.0f };
}

Texture2D ColorTexture <
    string ResourceName = "default_color.dds";
    string UIName =  "Color Texture";
    string ResourceType = "2D";
>;

Texture2D NormalMap <
    string ResourceName = "default_bump_normal.dds";
    string UIName =  "Normap Map";
    string ResourceType = "2D";
>;

SamplerState TrilinearSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = WRAP;
    AddressV = WRAP;
};

RasterizerState DisableCulling
{
    CullMode = NONE;
};

struct LIGHT_CONTRIBUTION_DATA
{
    float4 Color;
    float3 Normal;
    float3 ViewDirection;
    float4 LightColor;
    float4 LightDirection;
    float4 SpecularColor;
    float SpecularPower;
    float Attenuation;
};


float2 get_corrected_texture_coordinate(float2 textureCoordinate)
{
    #if FLIP_TEXTURE_Y
        return float2(textureCoordinate.x, 1.0 - textureCoordinate.y); 
    #else
        return textureCoordinate;
    #endif
}

float3 get_scalar_color_contribution(float4 light, float color)
{
    // Color (.rgb) * Intensity (.a)
    return light.rgb * light.a * color;
}

float3 get_vector_color_contribution(float4 light, float3 color)
{
    // Color (.rgb) * Intensity (.a)
    return light.rgb * light.a * color;
}

float3 get_light_contribution(LIGHT_CONTRIBUTION_DATA IN)
{
    float3 lightDirection = IN.LightDirection.xyz;
    float n_dot_l = dot(IN.Normal, lightDirection);
    float3 halfVector = normalize(lightDirection + IN.ViewDirection);
    float n_dot_h = dot(IN.Normal, halfVector);
    
    float4 lightCoefficients = lit(n_dot_l, n_dot_h, IN.SpecularPower);
    float3 diffuse = get_vector_color_contribution(IN.LightColor, lightCoefficients.y * IN.Color.rgb) * IN.LightDirection.w;
    float3 specular = get_scalar_color_contribution(IN.SpecularColor, min(lightCoefficients.z, IN.Color.w)) * IN.LightDirection.w * IN.LightColor.w;
    
    return (diffuse*IN.Attenuation + specular*IN.Attenuation);
}

/************* Data Structures *************/

struct VS_INPUT
{
    float4 ObjectPosition : POSITION;
    float2 TextureCoordinate : TEXCOORD;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
    float2 TextureCoordinate : TEXCOORD0;
    float3 LightDirection : TEXCOORD1;
    float3 ViewDirection : TEXCOORD2;
    float Attenuation : TEXCOORD3;
};

/************* Vertex Shader *************/

VS_OUTPUT vertex_shader(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;
    
    OUT.Position = mul(IN.ObjectPosition, WorldViewProjection);
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), World).xyz);
    OUT.Tangent = normalize(mul(float4(IN.Tangent, 0), World).xyz);
    OUT.Binormal = cross(OUT.Normal, OUT.Tangent);
    OUT.TextureCoordinate = get_corrected_texture_coordinate(IN.TextureCoordinate);
    
    //OUT.LightDirection = normalize(-LightDirection);
    
    float3 worldPosition = mul(IN.ObjectPosition, World).xyz;
    float3 viewDirection = CameraPosition - worldPosition;
    OUT.ViewDirection = normalize(viewDirection);

    OUT.LightDirection = LightPosition - worldPosition;

    OUT.Attenuation = saturate(1.0f - (length(OUT.LightDirection) / LightRadius)); // Attenuation	
        
    return OUT;
}

/************* Pixel Shader *************/

float4 pixel_shader(VS_OUTPUT IN) : SV_Target
{
    float4 OUT = (float4) 0;
    
    float3 sampledNormal = (2 * NormalMap.Sample(TrilinearSampler, IN.TextureCoordinate).xyz) - 1.0; // Map normal from [0..1] to [-1..1]
    float3x3 tbn = float3x3(IN.Tangent, IN.Binormal, IN.Normal);

    sampledNormal = mul(sampledNormal, tbn); // Transform normal to world space
    
    float3 viewDirection = normalize(IN.ViewDirection);
    float4 color = ColorTexture.Sample(TrilinearSampler, IN.TextureCoordinate);
    float3 ambient = get_vector_color_contribution(AmbientColor, color.rgb);
    
    LIGHT_CONTRIBUTION_DATA lightContributionData;
    lightContributionData.Color = color;
    lightContributionData.Normal = sampledNormal;
    lightContributionData.ViewDirection = viewDirection;
    lightContributionData.LightDirection = float4(IN.LightDirection, 1);
    lightContributionData.SpecularColor = SpecularColor;
    lightContributionData.SpecularPower = SpecularPower;
    lightContributionData.LightColor = LightColor;
    lightContributionData.Attenuation = IN.Attenuation; //attenuation
    float3 light_contribution = get_light_contribution(lightContributionData);
    
    OUT.rgb = ambient + light_contribution;
    OUT.a = 1.0f;
    
    return OUT;
}

/************* Techniques *************/

technique10 main10
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_4_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, pixel_shader()));
            
        SetRasterizerState(DisableCulling);
    }
}