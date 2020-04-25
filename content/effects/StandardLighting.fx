// Standard Lighting Effect
static const float4 ColorWhite = { 1, 1, 1, 1 };
static const float Pi = 3.141592654f;
cbuffer CBufferPerFrame
{
    float4 SunDirection;
    float4 SunColor;
    float4 AmbientColor;
    float4 ShadowTexelSize;
}
cbuffer CBufferPerObject
{
    float4x4 WorldViewProjection : WORLDVIEWPROJECTION;
    float4x4 World : WORLD;
    float4x4 ModelToShadow;
    float4 CameraPosition;
}

Texture2D AlbedoTexture;
Texture2D NormalTexture;
Texture2D SpecularTexture;
Texture2D MetallicTexture;
Texture2D RoughnessTexture;

Texture2D ShadowTexture;

TextureCube IrradianceTexture;
TextureCube RadianceTexture;
Texture2D IntegrationTexture;

SamplerState Sampler
{
    Filter = ANISOTROPIC;
    MaxAnisotropy = 16;
    AddressU = Wrap;
    AddressV = Wrap;
};

SamplerComparisonState ShadowSampler
{
    Filter = COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    AddressU = CLAMP;
    AddressV = CLAMP;
    BorderColor = ColorWhite;
    ComparisonFunc = LESS_EQUAL;
};

struct LightData
{
    float3 Pos;
    float RadiusSq;
    float3 Color;
    //float4x4 shadowTextureMatrix;
};

struct VS_INPUT
{
    float4 Position : POSITION;
    float2 Texcoord0 : TEXCOORD;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float3 WorldPos : WorldPos;
    float2 UV : TexCoord0;
    float3 ViewDir : TexCoord1;
    float3 ShadowCoord : TexCoord2;
    float3 Normal : Normal;
    float3 Tangent : Tangent;
};


VS_OUTPUT mainVS(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;

    OUT.Position = mul(IN.Position, WorldViewProjection);
    OUT.WorldPos = mul(IN.Position, World).xyz;
    OUT.UV = IN.Texcoord0;
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), World).xyz);
    OUT.ViewDir = IN.Position.xyz - CameraPosition.xyz;
    OUT.ShadowCoord = mul(IN.Position, ModelToShadow).xyz;
    OUT.Tangent = IN.Tangent;

    return OUT;
}

float GetShadow(float3 ShadowCoord)
{
    //float result = ShadowTexture.SampleCmpLevelZero(ShadowSampler, ShadowCoord.xy, ShadowCoord.z).r;

    const float Dilation = 2.0;
    float d1 = Dilation * ShadowTexelSize.x * 0.125;
    float d2 = Dilation * ShadowTexelSize.x * 0.875;
    float d3 = Dilation * ShadowTexelSize.x * 0.625;
    float d4 = Dilation * ShadowTexelSize.x * 0.375;
    float result = (
        2.0 * 
        ShadowTexture.SampleCmpLevelZero(ShadowSampler, ShadowCoord.xy, ShadowCoord.z) +
        ShadowTexture.SampleCmpLevelZero(ShadowSampler, ShadowCoord.xy + float2(-d2, d1), ShadowCoord.z) +
        ShadowTexture.SampleCmpLevelZero(ShadowSampler, ShadowCoord.xy + float2(-d1, -d2), ShadowCoord.z) +
        ShadowTexture.SampleCmpLevelZero(ShadowSampler, ShadowCoord.xy + float2(d2, -d1), ShadowCoord.z) +
        ShadowTexture.SampleCmpLevelZero(ShadowSampler, ShadowCoord.xy + float2(d1, d2), ShadowCoord.z) +
        ShadowTexture.SampleCmpLevelZero(ShadowSampler, ShadowCoord.xy + float2(-d4, d3), ShadowCoord.z) +
        ShadowTexture.SampleCmpLevelZero(ShadowSampler, ShadowCoord.xy + float2(-d3, -d4), ShadowCoord.z) +
        ShadowTexture.SampleCmpLevelZero(ShadowSampler, ShadowCoord.xy + float2(d4, -d3), ShadowCoord.z) +
        ShadowTexture.SampleCmpLevelZero(ShadowSampler, ShadowCoord.xy + float2(d3, d4), ShadowCoord.z)
        ) / 10.0;

    return result * result;
}

// Apply fresnel to modulate the specular albedo
void FSchlick(inout float3 specular, inout float3 diffuse, float3 lightDir, float3 halfVec)
{
    float fresnel = pow(1.0 - saturate(dot(lightDir, halfVec)), 5.0);
    specular = lerp(specular, 1, fresnel);
    diffuse = lerp(diffuse, 0, fresnel);
}

float3 ApplyLightCommon(float3 diffuseColor, float3 specularColor, float specularMask, float gloss, float3 normal, float3 viewDir, float3 lightDir, float3 lightColor)
{
    float3 halfVec = normalize(lightDir - viewDir);
    float nDotH = saturate(dot(halfVec, normal));

    FSchlick(diffuseColor, specularColor, lightDir, halfVec);

    float specularFactor = specularMask * pow(nDotH, gloss) * (gloss + 2) / 8;

    float nDotL = saturate(dot(normal, lightDir));
    return nDotL * lightColor * (diffuseColor + specularFactor * specularColor);
}

void AntiAliasSpecular(inout float3 texNormal, inout float gloss)
{
    float normalLenSq = dot(texNormal, texNormal);
    float invNormalLen = rsqrt(normalLenSq);
    texNormal *= invNormalLen;
    gloss = lerp(1, gloss, rcp(invNormalLen));
}

float3 ApplyAmbientLight(float3 diffuse, float ao, float3 lightColor)
{
    return ao * diffuse * lightColor;
}

float3 ApplyDirectionalLight(float3 diffuseColor,float3 specularColor, float specularMask, float gloss, float3 normal,
    float3 viewDir, float3 lightDir, float3 lightColor, float3 shadowCoord)
{
    float shadow = GetShadow(shadowCoord);
    return shadow * ApplyLightCommon(diffuseColor, specularColor, specularMask, gloss, normal, viewDir, lightDir, lightColor);
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
float3 Schlick_Fresnel_Roughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness, 1.0f - roughness, 1.0f - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
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
    float3 viewDir = normalize(CameraPosition.xyz - positionWS);
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

float3 DirectLightingPBR(float3 normalWS, float3 lightColor, float3 diffuseAlbedo,
	float3 specularAlbedo, float3 positionWS, float roughness, float attenuation)
{
    float3 lighting = 0.0f;
    float3 lightDir = SunDirection.xyz;

    float nDotL = saturate(dot(normalWS, lightDir));

    if (nDotL > 0.0f)
    {
        lighting = DirectDiffuseBRDF(diffuseAlbedo, nDotL) * attenuation + DirectSpecularBRDF(specularAlbedo, positionWS, normalWS, lightDir, roughness) * attenuation;
    }

    return max(lighting, 0.0f) * lightColor;
}

// ================================================================================================
// Split sum approximation 
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// ================================================================================================
float3 ApproximateSpecularIBL(float3 specularAlbedo, float3 reflectDir, float nDotV, float roughness)
{
	// Mip level is in [0, 6] range and roughness is [0, 1].
    float mipIndex = roughness * 6;
    float3 prefilteredColor = RadianceTexture.SampleLevel(Sampler, reflectDir, mipIndex);
    float3 environmentBRDF = IntegrationTexture.Sample(Sampler, float2(roughness, nDotV));

    return prefilteredColor * (specularAlbedo * environmentBRDF.x + environmentBRDF.y);

}
float3 IndirectLighting(float roughness, float3 diffuseAlbedo, float3 specularAlbedo, float3 normalWS, float3 positionWS)
{
    float3 viewDir = normalize(CameraPosition.xyz - positionWS);
    float3 reflectDir = normalize(reflect(-viewDir, normalWS));
    float nDotV = max(dot(normalWS, viewDir), 0.0001f);

	// Sample the indirect diffuse lighting from the irradiance environment map. 
    float3 indirectDiffuseLighting = IrradianceTexture.SampleLevel(Sampler, normalWS, 0).rgb * diffuseAlbedo;
	
    // Split sum approximation of specular lighting.
    float3 indirectSpecularLighting = ApproximateSpecularIBL(specularAlbedo, reflectDir, nDotV, roughness);

    return indirectDiffuseLighting + indirectSpecularLighting;
}

float3 mainPS(VS_OUTPUT vsOutput) : SV_Target0
{
    uint2 pixelPos = vsOutput.Position.xy;
    float4 diffuseAlbedo = AlbedoTexture.Sample(Sampler, vsOutput.UV);
    clip(diffuseAlbedo.a < 0.1f ? -1 : 1);
    float3 colorSum = 0;
    {
        float ao = 1;//texSSAO[ pixelPos];
        colorSum += ApplyAmbientLight(diffuseAlbedo.rgb, ao, AmbientColor.xyz);
    }

    float gloss = 128.0;
    float3 normal;
    {
        normal = NormalTexture.Sample(Sampler, vsOutput.UV) * 2.0 - 1.0;
        AntiAliasSpecular(normal, gloss);
        float3 bitangent = cross(vsOutput.Normal, vsOutput.Tangent);
        float3x3 tbn = float3x3(vsOutput.Tangent, bitangent, vsOutput.Normal);
        normal = normalize(mul(normal, tbn));
    }
    
    //float3 specularAlbedo = float3(0.56, 0.56, 0.56);
    float metalness = MetallicTexture.Sample(Sampler, vsOutput.UV).r;
    float3 specularAlbedo = float3(metalness, metalness, metalness);
    float specularMask = SpecularTexture.Sample(Sampler, vsOutput.UV).g;
    float3 viewDir = normalize(vsOutput.ViewDir);
    colorSum += ApplyDirectionalLight(diffuseAlbedo.rgb, specularAlbedo, specularMask, gloss, normal, viewDir, SunDirection.xyz, SunColor.xyz, vsOutput.ShadowCoord);

    //// point
    //for (uint n = 0; n < tileLightCountSphere; n++, tileLightLoadOffset += 4)
    //{
    //    uint lightIndex = lightGrid.Load(tileLightLoadOffset);
    //    LightData lightData = lightBuffer[lightIndex];
    //    colorSum += ApplyPointLight(POINT_LIGHT_ARGS);
    //}

    return colorSum;
}

float3 mainPS_PBR(VS_OUTPUT vsOutput) : SV_Target0
{

    float3 sampledNormal = (2 * NormalTexture.Sample(Sampler, vsOutput.UV).xyz) - 1.0; // Map normal from [0..1] to [-1..1]
    float3x3 tbn = float3x3(vsOutput.Tangent, cross(vsOutput.Normal, vsOutput.Tangent), vsOutput.Normal);
    sampledNormal = mul(sampledNormal, tbn); // Transform normal to world space

    float3 normalWS = sampledNormal;

    float4 diffuseAlbedo = pow(AlbedoTexture.Sample(Sampler, vsOutput.UV), 2.2);
    clip(diffuseAlbedo.a < 0.1f ? -1 : 1);
    float  metalness = MetallicTexture.Sample(Sampler, vsOutput.UV).r;
    float roughness = RoughnessTexture.Sample(Sampler, vsOutput.UV).r;
        
    float3 specularAlbedo = float3(metalness, metalness, metalness);

    float3 directLighting = DirectLightingPBR(normalWS, SunColor.xyz, diffuseAlbedo.rgb,
		specularAlbedo, vsOutput.WorldPos, roughness, 1.0f);
    
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, diffuseAlbedo.rgb, metalness);
    specularAlbedo = Schlick_Fresnel_Roughness(max(dot(normalWS, normalize(CameraPosition.xyz - vsOutput.WorldPos)), 0.0001f), F0, roughness);
    
    float3 indirectLighting = IndirectLighting(roughness, diffuseAlbedo.rgb,
		specularAlbedo, normalWS, vsOutput.WorldPos);

    float shadow = GetShadow(vsOutput.ShadowCoord);
    float3 ambient = AmbientColor.rgb * diffuseAlbedo.rgb;
	return ambient + float3(directLighting + indirectLighting) * shadow;
}

/************* Techniques *************/

technique11 standard_lighting_no_pbr
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, mainVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, mainPS()));
    }
}

technique11 standard_lighting_pbr
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, mainVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, mainPS_PBR()));
    }
}