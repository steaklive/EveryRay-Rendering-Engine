// Standard Lighting Effect
static const float4 ColorWhite = { 1, 1, 1, 1 };
static const float Pi = 3.141592654f;

#define NUM_OF_SHADOW_CASCADES 3

cbuffer CBufferPerFrame
{
    float4 SunDirection;
    float4 SunColor;
    float4 AmbientColor;
    float4 ShadowTexelSize;
    float4 ShadowCascadeDistances;
}
cbuffer CBufferPerObject
{
    float4x4 ViewProjection;
    float4x4 World : WORLD;
    float4x4 ShadowMatrices[NUM_OF_SHADOW_CASCADES];
    float4 CameraPosition;
}

Texture2D AlbedoTexture;
Texture2D NormalTexture;
Texture2D SpecularTexture;
Texture2D MetallicTexture;
Texture2D RoughnessTexture;

Texture2D CascadedShadowTextures[NUM_OF_SHADOW_CASCADES];

TextureCube IrradianceDiffuseTexture;
TextureCube IrradianceSpecularTexture;
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
SamplerComparisonState CascadedPcfShadowMapSampler
{
    Filter = COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    AddressU = BORDER;
    AddressV = BORDER;
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

struct VS_INPUT_INSTANCING
{
    float4 Position : POSITION;
    float2 Texcoord0 : TEXCOORD;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    
    //instancing
    row_major float4x4 World : WORLD; 
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float3 WorldPos : WorldPos;
    float2 UV : TexCoord0;
    float3 ViewDir : TexCoord1;
    float3 ShadowCoord0 : TexCoord2;
    float3 ShadowCoord1 : TexCoord3;
    float3 ShadowCoord2 : TexCoord4;
    float3 Normal : Normal;
    float3 Tangent : Tangent;
};


VS_OUTPUT mainVS(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;

    OUT.Position = mul(IN.Position, mul(World, ViewProjection));
    OUT.WorldPos = mul(IN.Position, World).xyz;
    OUT.UV = IN.Texcoord0;
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), World).xyz);
    OUT.ViewDir = IN.Position.xyz - CameraPosition.xyz;
    OUT.ShadowCoord0 = mul(IN.Position, mul(World, ShadowMatrices[0])).xyz;
    OUT.ShadowCoord1 = mul(IN.Position, mul(World, ShadowMatrices[1])).xyz;
    OUT.ShadowCoord2 = mul(IN.Position, mul(World, ShadowMatrices[2])).xyz;
    OUT.Tangent = IN.Tangent;

    return OUT;
}

VS_OUTPUT mainVS_Instancing(VS_INPUT_INSTANCING IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;

    OUT.WorldPos = mul(IN.Position, IN.World).xyz;
    OUT.Position = mul(float4(OUT.WorldPos, 1.0f), ViewProjection);
    OUT.UV = IN.Texcoord0;
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), IN.World).xyz);
    OUT.ViewDir = IN.Position.xyz - CameraPosition.xyz;
    OUT.ShadowCoord0 = mul(IN.Position, mul(IN.World, ShadowMatrices[0])).xyz;
    OUT.ShadowCoord1 = mul(IN.Position, mul(IN.World, ShadowMatrices[1])).xyz;
    OUT.ShadowCoord2 = mul(IN.Position, mul(IN.World, ShadowMatrices[2])).xyz;
    OUT.Tangent = IN.Tangent;

    return OUT;
}

float CalculateShadow(float3 ShadowCoord, int index)
{
    const float Dilation = 2.0;
    float d1 = Dilation * ShadowTexelSize.x * 0.125;
    float d2 = Dilation * ShadowTexelSize.x * 0.875;
    float d3 = Dilation * ShadowTexelSize.x * 0.625;
    float d4 = Dilation * ShadowTexelSize.x * 0.375;
    float result = (
        2.0 *
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy, ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d2, d1), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d1, -d2), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d2, -d1), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d1, d2), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d4, d3), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d3, -d4), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d4, -d3), ShadowCoord.z) +
        CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d3, d4), ShadowCoord.z)
        ) / 10.0;

    return result * result;
}
float GetShadow(float3 ShadowCoord0, float3 ShadowCoord1, float3 ShadowCoord2, float depthDistance)
{
    if (depthDistance < ShadowCascadeDistances.x)
        return CalculateShadow(ShadowCoord0, 0);
    else if (depthDistance < ShadowCascadeDistances.y)
        return CalculateShadow(ShadowCoord1, 1);
    else if (depthDistance < ShadowCascadeDistances.z)
        return CalculateShadow(ShadowCoord2, 2);
    else
        return 1.0f;
    
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
    float3 viewDir, float3 lightDir, float3 lightColor, float3 shadowCoord0,float3 shadowCoord1,float3 shadowCoord2, float depthPosition)
{
    float shadow = GetShadow(shadowCoord0, shadowCoord1, shadowCoord2, depthPosition);
    return shadow * ApplyLightCommon(diffuseColor, specularColor, specularMask, gloss, normal, viewDir, lightDir, lightColor);
}

// ===============================================================================================
// http://graphicrants.blogspot.com.au/2013/08/specular-brdf-reference.html
// ===============================================================================================

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = Pi * denom * denom;

    return nom / max(denom, 0.001); // prevent divide by zero for roughness=0.0 and NdotH=1.0
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// ================================================================================================
// Fresnel with Schlick's approximation:
// http://blog.selfshadow.com/publications/s2013-shading-course/rad/s2013_pbs_rad_notes.pdf
// http://graphicrants.blogspot.fr/2013/08/specular-brdf-reference.html
// ================================================================================================
float3 Schlick_Fresnel(float3 f0, float cosTheta)
{
    return f0 + (1.0f - f0) * pow((1.0f - cosTheta), 5.0f);
}
float3 Schlick_Fresnel_Roughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

// ================================================================================================
float3 DirectDiffuseBRDF(float3 diffuseAlbedo, float3 lightDir, float3 viewDir, float3 F0, float metallic)
{
    float3 halfVec = normalize(viewDir + lightDir);
   
    float3 F = Schlick_Fresnel(F0, max(dot(halfVec, viewDir), 0.0));
    float3 kS = F;
    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - kS.
    float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
    // multiply kD by the inverse metalness such that only non-metals 
    // have diffuse lighting, or a linear blend if partly metal (pure metals
    // have no diffuse light).
    kD *= (float3(1.0f, 1.0f, 1.0f) - metallic);
    
    return (diffuseAlbedo * kD) / Pi;
}

float3 DirectDiffuseBRDF(float3 diffuseAlbedo, float nDotL)
{
    return (diffuseAlbedo * nDotL) / Pi;
}

// ================================================================================================
// Cook-Torrence BRDF
float3 DirectSpecularBRDF(float3 normalWS, float3 lightDir, float3 viewDir, float roughness, float3 F0)
{
    float3 halfVec = normalize(viewDir + lightDir);
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(normalWS, halfVec, roughness);
    float G = GeometrySmith(normalWS, viewDir, lightDir, roughness);
    float3 F = Schlick_Fresnel(F0, max(dot(halfVec, viewDir), 0.0));
           
    float3 nominator = NDF * G * F;
    float denominator = 4 * max(dot(normalWS, viewDir), 0.0) * max(dot(normalWS, lightDir), 0.0);
    float3 specular = nominator / max(denominator, 0.001); // prevent divide by zero for NdotV=0.0 or NdotL=0.0
       
    return specular;
}
// ================================================================================================

float3 DirectLightingPBR(float3 normalWS, float3 lightColor, float3 diffuseAlbedo,
	float3 specularAlbedo, float3 positionWS, float roughness, float3 F0, float metallic)
{
    float3 lightDir = SunDirection.xyz;
    float3 viewDir = normalize(CameraPosition.xyz - positionWS).rgb;
    float nDotL = max(dot(normalWS, lightDir), 0.0);

    //float3 lighting = DirectDiffuseBRDF(diffuseAlbedo, lightDir, viewDir, F0, metallic) + DirectSpecularBRDF(normalWS, lightDir, viewDir, roughness, F0);
    //return lighting * nDotL * lightColor;
    
    float3 lighting = DirectDiffuseBRDF(diffuseAlbedo, lightDir, viewDir, F0, metallic) + DirectSpecularBRDF(normalWS, lightDir, viewDir, roughness, F0);

    return max(lighting, 0.0f) * nDotL * lightColor;
}

// ================================================================================================
// Split sum approximation 
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// ================================================================================================
float3 ApproximateSpecularIBL(float3 F0, float3 reflectDir, float nDotV, float roughness)
{
    uint width, height, levels;
    IrradianceSpecularTexture.GetDimensions(0, width, height, levels);
    float mipIndex = roughness * levels;
    
    float3 prefilteredColor = IrradianceSpecularTexture.SampleLevel(Sampler, reflectDir, mipIndex);
    float3 environmentBRDF = IntegrationTexture.Sample(Sampler, float2(nDotV, roughness));

    return prefilteredColor * (F0 * environmentBRDF.x + environmentBRDF.y);

}
float3 IndirectLightingPBR(float roughness, float3 diffuseAlbedo, float3 specularAlbedo, float3 normalWS, float3 positionWS, float metalness, float3 F0)
{
    float3 viewDir = normalize(CameraPosition.xyz - positionWS);
    float nDotV = max(dot(normalWS, viewDir), 0.0001f);
    float3 reflectDir = reflect(-viewDir, normalWS);

	// Sample the indirect diffuse lighting from the irradiance environment map. 
    float3 irradiance = IrradianceDiffuseTexture.SampleLevel(Sampler, normalWS, 0).rgb;
    float3 F = Schlick_Fresnel_Roughness(nDotV, F0, roughness);
    float3 kD = float3(1.0f, 1.0f, 1.0f) - F;
    kD *= (float3(1.0f, 1.0f, 1.0f) - metalness);
    float3 indirectDiffuseLighting = /*kD * */irradiance * diffuseAlbedo;

    // Split sum approximation of specular lighting.
    float3 indirectSpecularLighting = ApproximateSpecularIBL(F0, reflectDir, nDotV, roughness);
    
    return (indirectDiffuseLighting + indirectSpecularLighting) /*ao*/;
}

float3 mainPS(VS_OUTPUT vsOutput) : SV_Target0
{
    uint2 pixelPos = vsOutput.Position.xy;
    float4 diffuseAlbedo = AlbedoTexture.Sample(Sampler, vsOutput.UV);
    clip(diffuseAlbedo.a < 0.1f ? -1 : 1);
    float3 colorSum = 0;
    {
        float ao = 1;
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
    colorSum += ApplyDirectionalLight(diffuseAlbedo.rgb, specularAlbedo, specularMask, gloss, normal, viewDir, SunDirection.xyz, SunColor.xyz, vsOutput.ShadowCoord0, vsOutput.ShadowCoord1, vsOutput.ShadowCoord2, vsOutput.Position.w);

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
    float metalness = MetallicTexture.Sample(Sampler, vsOutput.UV).r;
    float roughness = RoughnessTexture.Sample(Sampler, vsOutput.UV).r;
        
    float3 specularAlbedo = float3(metalness, metalness, metalness);

    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, diffuseAlbedo.rgb, metalness);

    float3 directLighting = DirectLightingPBR(normalWS, SunColor.xyz, diffuseAlbedo.rgb, specularAlbedo, vsOutput.WorldPos, roughness, F0, metalness);
    float3 indirectLighting = IndirectLightingPBR(roughness, diffuseAlbedo.rgb, specularAlbedo, normalWS, vsOutput.WorldPos, metalness, F0);

    float shadow = GetShadow(vsOutput.ShadowCoord0, vsOutput.ShadowCoord1, vsOutput.ShadowCoord2, vsOutput.Position.w);
    
    float indirectLightingContribution = 0.8f;
    float3 color = (directLighting * shadow + indirectLighting * indirectLightingContribution);
            
    // HDR tonemapping
    color = color / (color + float3(1.0f, 1.0f, 1.0f));
    return color;
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

technique11 standard_lighting_no_pbr_instancing
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, mainVS_Instancing()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, mainPS()));
    }
}

technique11 standard_lighting_pbr_instancing
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, mainVS_Instancing()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, mainPS_PBR()));
    }
}