//*****************************************************************************//
//*****************************************************************************//
//                      Simple PBR + IBL implementation                        //
//*****************************************************************************//
//*****************************************************************************//

#define FLIP_TEXTURE_Y 0
static const float Pi = 3.141592654f;

cbuffer CBufferPerFrame
{
    float3 LightPosition;
    float LightRadius;
    float3 CameraPosition;
    
    float Roughness;
    float Metalness;
}

cbuffer CBufferPerObject
{
    float4x4 WorldViewProjection : WORLDVIEWPROJECTION;
    float4x4 World : WORLD;
}

// Albedo, normal, roughness, metalness, ao maps
Texture2D albedoTexture;
Texture2D roughnessTexture;
Texture2D metallicTexture;
Texture2D normalTexture;
Texture2D aoTexture;

TextureCube irradianceTexture;
TextureCube radianceTexture;
Texture2D integrationTexture;

SamplerState ColorSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = WRAP;
    AddressV = WRAP;
};

SamplerState SamplerAnisotropic
{
    Filter = ANISOTROPIC;
    MaxAnisotropy = 16;
    AddressU = Wrap;
    AddressV = Wrap;
};

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
    float3 WorldPosition : TEXCOORD1;
    float Attenuation : TEXCOORD2;


};
RasterizerState BackFaceCulling
{
    CullMode = BACK;
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

// ===============================================================================================
// http://graphicrants.blogspot.com.au/2013/08/specular-brdf-reference.html
// ===============================================================================================
float GGX(float NdotV, float a)
{
    float k = a / 2;
    return NdotV / (NdotV * (1.0f - k) + k);
}

// ===============================================================================================
// Geometry with Smith approximation:
// http://blog.selfshadow.com/publications/s2013-shading-course/rad/s2013_pbs_rad_notes.pdf
// http://graphicrants.blogspot.fr/2013/08/specular-brdf-reference.html
// ===============================================================================================
float G_Smith(float a, float nDotV, float nDotL)
{
    return GGX(nDotL, a * a) * GGX(nDotV, a * a);
}

// ================================================================================================
// Fresnel with Schlick's approximation:
// http://blog.selfshadow.com/publications/s2013-shading-course/rad/s2013_pbs_rad_notes.pdf
// http://graphicrants.blogspot.fr/2013/08/specular-brdf-reference.html
// ================================================================================================
float3 Schlick_Fresnel(float3 f0, float3 h, float3 l)
{
    return f0 + (1.0f - f0) * pow((1.0f - dot(l, h)), 5.0f);
}

// ================================================================================================
// Lambertian BRDF
// http://en.wikipedia.org/wiki/Lambertian_reflectance
// ================================================================================================
float3 DirectDiffuseBRDF(float3 diffuseAlbedo, float nDotL)
{
    return (diffuseAlbedo * nDotL) / Pi;
}

// ================================================================================================
// Cook-Torrence BRDF
float3 DirectSpecularBRDF(float3 specularAlbedo, float3 positionWS, float3 normalWS, float3 lightDir, float roughness)
{
    float3 viewDir = normalize(CameraPosition - positionWS);
    float3 halfVec = normalize(viewDir + lightDir);

    float nDotH = saturate(dot(normalWS, halfVec));
    float nDotL = saturate(dot(normalWS, lightDir));
    float nDotV = max(dot(normalWS, viewDir), 0.0001f);

    float alpha2 = roughness * roughness;

	// Normal distribution term with Trowbridge-Reitz/GGX.
    float D = alpha2 / (Pi * pow(nDotH * nDotH * (alpha2 - 1) + 1, 2.0f));
 
	// Fresnel term with Schlick's approximation.
    float3 F = Schlick_Fresnel(specularAlbedo, halfVec, lightDir);

	// Geometry term with Smith's approximation.
    float G = G_Smith(roughness, nDotV, nDotL);

    return D * F * G;
}
// ================================================================================================


/************* Vertex Shader *************/

VS_OUTPUT vertex_shader(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;

    OUT.Position = mul(IN.ObjectPosition, WorldViewProjection);
    OUT.WorldPosition = mul(IN.ObjectPosition, World).xyz;
    OUT.TextureCoordinate = get_corrected_texture_coordinate(IN.TextureCoordinate);
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), World).xyz);
    OUT.Tangent = normalize(mul(float4(IN.Tangent, 0), World).xyz);
    OUT.Binormal = cross(OUT.Normal, OUT.Tangent);

    float3 lightDirection = LightPosition - OUT.WorldPosition;
    OUT.Attenuation = saturate(1.0f - (length(lightDirection) / LightRadius)); // Attenuation	


    return OUT;
}

// ================================================================================================
// Split sum approximation 
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// ================================================================================================
float3 ApproximateSpecularIBL(float3 specularAlbedo, float3 reflectDir, float nDotV, float roughness)
{
	// Mip level is in [0, 6] range and roughness is [0, 1].
    float mipIndex = roughness * 6;

    float3 prefilteredColor = radianceTexture.SampleLevel(SamplerAnisotropic, reflectDir, mipIndex);
    float3 environmentBRDF = integrationTexture.Sample(SamplerAnisotropic, float2(roughness, nDotV));

    return prefilteredColor * (specularAlbedo * environmentBRDF.x + environmentBRDF.y);
}


float3 DirectLighting(float3 normalWS, float3 lightColor, float3 lightPos, float3 diffuseAlbedo,
	float3 specularAlbedo, float3 positionWS, float roughness, float attenuation)
{
    float3 lighting = 0.0f;

    float3 pixelToLight = lightPos - positionWS;
    float lightDist = length(pixelToLight);
    float3 lightDir = pixelToLight / lightDist;

    float nDotL = saturate(dot(normalWS, lightDir));

    if (nDotL > 0.0f)
    {
        lighting = DirectDiffuseBRDF(diffuseAlbedo, nDotL)*attenuation + DirectSpecularBRDF(specularAlbedo, positionWS, normalWS, lightDir, roughness)*attenuation;
    }

    return max(lighting, 0.0f) * lightColor;
}

float3 IndirectLighting(float roughness, float3 diffuseAlbedo, float3 specularAlbedo, float3 normalWS, float3 positionWS)
{
    float3 viewDir = normalize(CameraPosition - positionWS);
    float3 reflectDir = normalize(reflect(-viewDir, normalWS));
    float nDotV = max(dot(normalWS, viewDir), 0.0001f);

	// Sample the indirect diffuse lighting from the irradiance environment map. 
    float3 indirectDiffuseLighting = irradianceTexture.SampleLevel(SamplerAnisotropic, normalWS, 0) * diffuseAlbedo;
	
    // Split sum approximation of specular lighting.
    float3 indirectSpecularLighting = ApproximateSpecularIBL(specularAlbedo, reflectDir, nDotV, roughness);

    return indirectDiffuseLighting + indirectSpecularLighting;
}

float4 pixel_shader(VS_OUTPUT IN) : SV_TARGET
{
    float3 sampledNormal = (2 * normalTexture.Sample(SamplerAnisotropic, IN.TextureCoordinate).xyz) - 1.0; // Map normal from [0..1] to [-1..1]
    float3x3 tbn = float3x3(IN.Tangent, IN.Binormal, IN.Normal);

    sampledNormal = mul(sampledNormal, tbn); // Transform normal to world space

    float3 normalWS = sampledNormal;

    //float3 diffuseAlbedo = albedoTexture.Sample(ColorSampler, IN.TextureCoordinate);
    float3 diffuseAlbedo = pow(albedoTexture.Sample(SamplerAnisotropic, IN.TextureCoordinate), 2.2);
    
    //cheap trick if you need a custom metalness/roughness
    float metalness = 1.0;
    if (Roughness < 0)
    {
        metalness = Metalness;
    }
    else
    {
        metalness = metallicTexture.Sample(SamplerAnisotropic, IN.TextureCoordinate).r;
    }

    float roughness = 1.0;
    if (Metalness < 0)
    {
        roughness = Roughness;
    }
    else
    {
        roughness = roughnessTexture.Sample(SamplerAnisotropic, IN.TextureCoordinate).r;
    }
    

    float3 specularAlbedo = float3(metalness, metalness, metalness);

    float3 lightPos = LightPosition;
    float3 lightColor = float3(0.8f, 0.8f, 0.8f);

    float3 directLighting = DirectLighting(normalWS, lightColor, lightPos, diffuseAlbedo,
		specularAlbedo, IN.WorldPosition, roughness, IN.Attenuation);

    float3 indirectLighting = IndirectLighting(roughness, diffuseAlbedo,
		specularAlbedo, normalWS, IN.WorldPosition);

     return float4(directLighting + indirectLighting, 1);
    
}

/************* Techniques *************/

technique11 pbr_material
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, pixel_shader()));

        SetRasterizerState(BackFaceCulling);
    }
}