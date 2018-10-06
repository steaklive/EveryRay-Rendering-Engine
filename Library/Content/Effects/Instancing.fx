
/************* Resources *************/

cbuffer CBufferPerFrame
{
    float4 LightColor0 = { 1.0f, 1.0f, 1.0f, 1.0f };
    float3 LightPosition0 = { 0.0f, 0.0f, 0.0f };

    float4 LightColor1 = { 1.0f, 1.0f, 1.0f, 1.0f };
    float3 LightPosition1 = { 0.0f, 0.0f, 0.0f };

    float4 LightColor2 = { 1.0f, 1.0f, 1.0f, 1.0f };
    float3 LightPosition2 = { 0.0f, 0.0f, 0.0f };

    float4 LightColor3 = { 1.0f, 1.0f, 1.0f, 1.0f };
    float3 LightPosition3 = { 0.0f, 0.0f, 0.0f };

    float LightRadius0 = 10.0f;
    float LightRadius1 = 10.0f;
    float LightRadius2 = 10.0f;
    float LightRadius3 = 10.0f;

    float3 CameraPosition : CAMERAPOSITION;
}

cbuffer CBufferPerObject
{
    float4x4 ViewProjection : VIEWPROJECTION;
}

Texture2D ColorTexture;
Texture2D NormalTexture;

SamplerState TrilinearSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = WRAP;
    AddressV = WRAP;
};

BlendState AlphaBlendingOn
{
    BlendEnable[0] = TRUE;
    DestBlend = INV_SRC_ALPHA;
    SrcBlend = SRC_ALPHA;
};

/************* Data Structures *************/

struct VS_INPUT
{
    float4 ObjectPosition : POSITION;
    float2 TextureCoordinate : TEXCOORD;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    row_major float4x4 World : WORLD;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
    float2 TextureCoordinate : TEXCOORD;
    float3 WorldPosition : WORLD;

    float Attenuation0 : ATTENUATION0;
    float Attenuation1 : ATTENUATION1;
    float Attenuation2 : ATTENUATION2;
    float Attenuation3 : ATTENUATION3;
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
    VS_OUTPUT OUT = (VS_OUTPUT) 0;
    
    OUT.WorldPosition = mul(IN.ObjectPosition, IN.World).xyz;
    OUT.Position = mul(float4(OUT.WorldPosition, 1.0f), ViewProjection);
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), IN.World).xyz);
    OUT.Tangent = normalize(mul(float4(IN.Tangent, 0), IN.World).xyz);
    OUT.Binormal = cross(OUT.Normal, OUT.Tangent);
    OUT.TextureCoordinate = IN.TextureCoordinate;

    OUT.Attenuation0 = saturate(1.0f - (length(LightPosition0 - OUT.WorldPosition) / LightRadius0));
    OUT.Attenuation1 = saturate(1.0f - (length(LightPosition1 - OUT.WorldPosition) / LightRadius1));
    OUT.Attenuation2 = saturate(1.0f - (length(LightPosition2 - OUT.WorldPosition) / LightRadius2));
    OUT.Attenuation3 = saturate(1.0f - (length(LightPosition3 - OUT.WorldPosition) / LightRadius3));

    return OUT;
}

/************* Pixel Shader *************/

float3 CalculateDiffuse(float3 lightPos, float4 lightColor, float3 worldPos, float3 normal, float4 color, float attenuation)
{
    float3 lightDirection = lightPos - worldPos;
    lightDirection = normalize(lightDirection);

    float3 viewDirection = normalize(CameraPosition - worldPos);

    float n_dot_l = dot(normal, lightDirection);
    float3 halfVector = normalize(lightDirection + viewDirection);
    float n_dot_h = dot(normal, halfVector);

    float4 lightCoefficients = lit(n_dot_l, n_dot_h, 0);

    float3 diffuse = get_vector_color_contribution(lightColor, lightCoefficients.y * color.rgb) * attenuation;

    return diffuse;
}

float4 pixel_shader(VS_OUTPUT IN) : SV_Target
{
    float4 OUT = (float4) 0;
    
    float4 color = ColorTexture.Sample(TrilinearSampler, IN.TextureCoordinate);
    
    //float3 normal = normalize(IN.Normal);
    float3 sampledNormal = (2 * NormalTexture.Sample(TrilinearSampler, IN.TextureCoordinate).xyz) - 1.0; // Map normal from [0..1] to [-1..1]
    float3x3 tbn = float3x3(IN.Tangent, IN.Binormal, IN.Normal);

    sampledNormal = mul(sampledNormal, tbn); // Transform normal to world space
    

    float3 diffuse = saturate(
        CalculateDiffuse(LightPosition0, LightColor0, IN.WorldPosition, sampledNormal, color, IN.Attenuation0)+
        CalculateDiffuse(LightPosition1, LightColor1, IN.WorldPosition, sampledNormal, color, IN.Attenuation1)+
        CalculateDiffuse(LightPosition2, LightColor2, IN.WorldPosition, sampledNormal, color, IN.Attenuation2)+
        CalculateDiffuse(LightPosition3, LightColor3, IN.WorldPosition, sampledNormal, color, IN.Attenuation3)
    );

    OUT.rgb = diffuse;
    OUT.a = color.a;
    
    return OUT;
}

/************* Techniques *************/

technique11 main11
{
    pass p0
    {
        SetBlendState(AlphaBlendingOn, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, pixel_shader()));
    }
}