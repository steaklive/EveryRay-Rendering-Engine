/************* Resources *************/
static const float4 ColorWhite = { 1, 1, 1, 1 };
static const float3 ColorBlack = { 0, 0, 0 };
static const float DepthBias = 0.005;

cbuffer CBufferPerFrame
{
    float4 AmbientColor = { 1.0f, 1.0f, 1.0f, 0.0f };
    float4 LightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float3 LightDirection = { 0.0f, 0.0f, 0.0f };
    float3 CameraPosition;
    float2 ShadowMapSize = { 2048.0f, 2048.0f };
}

cbuffer CBufferPerObject
{
    float4x4 WorldViewProjection : WORLDVIEWPROJECTION;
    float4x4 World : WORLD;

    float4x4 ProjectiveTextureMatrix0;
    float4x4 ProjectiveTextureMatrix1;
    float4x4 ProjectiveTextureMatrix2;
}

bool visualizeCascades;
float3 Distances;
Texture2D ColorTexture;
Texture2D ShadowMap0;
Texture2D ShadowMap1;
Texture2D ShadowMap2;

SamplerComparisonState PcfShadowMapSampler
{
    Filter = COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    AddressU = BORDER;
    AddressV = BORDER;
    BorderColor = ColorWhite;
    ComparisonFunc = LESS_EQUAL;
};

SamplerState ShadowMapSampler
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = BORDER;
    AddressV = BORDER;
    BorderColor = ColorWhite;
};

SamplerState ColorSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = WRAP;
    AddressV = WRAP;
};

RasterizerState BackFaceCulling
{
    CullMode = BACK;
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
    float3 LightDirection : TEXCOORD2;
    float4 ShadowTextureCoordinate0 : TEXCOORD3;
    float4 ShadowTextureCoordinate1 : TEXCOORD4;
    float4 ShadowTextureCoordinate2 : TEXCOORD5;

};

/************* Vertex Shader *************/

VS_OUTPUT vertex_shader(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;

    OUT.Position = mul(IN.ObjectPosition, WorldViewProjection);
    OUT.WorldPosition = mul(IN.ObjectPosition, World).xyz;
    OUT.TextureCoordinate = IN.TextureCoordinate;
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), World).xyz);

    OUT.LightDirection = normalize(-LightDirection);

    OUT.ShadowTextureCoordinate0 = mul(IN.ObjectPosition, ProjectiveTextureMatrix0);
    OUT.ShadowTextureCoordinate1 = mul(IN.ObjectPosition, ProjectiveTextureMatrix1);
    OUT.ShadowTextureCoordinate2 = mul(IN.ObjectPosition, ProjectiveTextureMatrix2);

    return OUT;
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

/************* Pixel Shaders *************/

float4 shadowmap_pcf_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    
    float4 OUT = (float4) 0;

    float3 lightDirection = normalize(IN.LightDirection);

    float3 viewDirection = normalize(CameraPosition - IN.WorldPosition);

    float3 normal = normalize(IN.Normal);
    float n_dot_l = dot(normal, lightDirection);
    float3 halfVector = normalize(lightDirection + viewDirection);
    float n_dot_h = dot(normal, halfVector);

    float4 color = ColorTexture.Sample(ColorSampler, IN.TextureCoordinate);

    float3 ambient = get_vector_color_contribution(AmbientColor, color.rgb);
    float3 diffuse = (float3) 0;
	
    if (n_dot_l > 0)
    {
        diffuse = LightColor.rgb * LightColor.a * n_dot_l * color.rgb;
    }

    float pixelDepth = IN.ShadowTextureCoordinate2.z;
    float shadow = ShadowMap2.SampleCmpLevelZero(PcfShadowMapSampler, IN.ShadowTextureCoordinate2.xy, pixelDepth).x;
    diffuse *= shadow;
    


    OUT.rgb = ambient + diffuse;
    OUT.a = 1.0f;

    return OUT;
}

float4 csm_shadowmap_pcf_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    
    float4 OUT = (float4) 0;

    float3 lightDirection = normalize(IN.LightDirection);

    float3 viewDirection = normalize(CameraPosition - IN.WorldPosition);

    float3 normal = normalize(IN.Normal);
    float n_dot_l = dot(normal, lightDirection);
    float3 halfVector = normalize(lightDirection + viewDirection);
    float n_dot_h = dot(normal, halfVector);

    float4 color = ColorTexture.Sample(ColorSampler, IN.TextureCoordinate);

    float3 ambient = get_vector_color_contribution(AmbientColor, color.rgb);
    float3 diffuse = (float3) 0;
	
    if (n_dot_l > 0)
    {
        diffuse = LightColor.rgb * LightColor.a * n_dot_l * color.rgb;
    }

    float depthTest = IN.Position.w;
    //float depthTest = -IN.WorldPosition.z;


    if (depthTest < Distances.x)
    {
        float pixelDepth = IN.ShadowTextureCoordinate0.z;
        float shadow = ShadowMap0.SampleCmpLevelZero(PcfShadowMapSampler, IN.ShadowTextureCoordinate0.xy, pixelDepth).x; 
        
        if (visualizeCascades)
            diffuse = shadow * float3(0.4f, 0.0f, 0.0f);
        else diffuse*=shadow;

    }
    
    else if (depthTest < Distances.y)
    {
        float pixelDepth = IN.ShadowTextureCoordinate1.z;
        float shadow = ShadowMap1.SampleCmpLevelZero(PcfShadowMapSampler, IN.ShadowTextureCoordinate1.xy, pixelDepth).x;
        
        if (visualizeCascades)
            diffuse = shadow * float3(0.2f, 0.4f, 0.0f);
        else diffuse *= shadow;

    }
    
    else if (depthTest < Distances.z)
    {
        float pixelDepth = IN.ShadowTextureCoordinate2.z;
        float shadow = ShadowMap2.SampleCmpLevelZero(PcfShadowMapSampler, IN.ShadowTextureCoordinate2.xy, pixelDepth).x;
        
        if (visualizeCascades)
            diffuse = shadow * float3(0.1f, 0.0f, 0.4f);
        else diffuse *= shadow;
    }


    OUT.rgb = (ambient + diffuse);
    OUT.a = 0.5f;

    return OUT;
}

/************* Techniques *************/

technique11 shadow_mapping_pcf
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, shadowmap_pcf_pixel_shader()));

        SetRasterizerState(BackFaceCulling);
    }
}

technique11 csm_shadow_mapping_pcf
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, csm_shadowmap_pcf_pixel_shader()));

        SetRasterizerState(BackFaceCulling);
    }
}