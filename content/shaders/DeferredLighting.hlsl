#define NUM_OF_SHADOW_CASCADES 3
static const float4 ColorWhite = { 1, 1, 1, 1 };
static const float Pi = 3.141592654f;

Texture2D<float4> GbufferAlbedoTexture : register(t0);
Texture2D<float4> GbufferNormalTexture : register(t1);
Texture2D<float4> GbufferWorldPosTexture : register(t2);
Texture2D<float4> GbufferExtraTexture : register(t3);
TextureCube<float4> IrradianceDiffuseTexture : register(t4);
TextureCube<float4> IrradianceSpecularTexture : register(t5);
Texture2D<float4> IntegrationTexture : register(t6);
Texture2D<float> CascadedShadowTextures[NUM_OF_SHADOW_CASCADES] : register(t7);

SamplerState SamplerLinear : register(s0);
SamplerComparisonState CascadedPcfShadowMapSampler : register(s1);

RWTexture2D<float4> OutputTexture : register(u0);

cbuffer DeferredLightingCBuffer : register(b0)
{
    float4x4 ShadowMatrices[NUM_OF_SHADOW_CASCADES];
    float4 ShadowCascadeDistances;
    float4 ShadowTexelSize;
    float4 SunDirection;
    float4 SunColor;
    float4 CameraPosition;
}

float CalculateShadow(float3 worldPos, float4x4 svp, int index)
{
    float4 lightSpacePos = mul(svp, float4(worldPos, 1.0f));
    float4 ShadowCoord = lightSpacePos / lightSpacePos.w;
    ShadowCoord.rg = ShadowCoord.rg * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    
    const float Dilation = 2.0;
    float d1 = Dilation * ShadowTexelSize.x * 0.125;
    float d2 = Dilation * ShadowTexelSize.x * 0.875;
    float d3 = Dilation * ShadowTexelSize.x * 0.625;
    float d4 = Dilation * ShadowTexelSize.x * 0.375;
    float result = (
        2.0 * CascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy, ShadowCoord.z) +
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
float GetShadow(float4 worldPos)
{
    float depthDistance = worldPos.a;
    
    if (depthDistance < ShadowCascadeDistances.x)
        return CalculateShadow(worldPos.rgb, ShadowMatrices[0], 0);
    else if (depthDistance < ShadowCascadeDistances.y)
        return CalculateShadow(worldPos.rgb, ShadowMatrices[1], 1);
    else if (depthDistance < ShadowCascadeDistances.z)
        return CalculateShadow(worldPos.rgb, ShadowMatrices[2], 2);
    else
        return 1.0f;   
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

    return nom / max(denom, 0.001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

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
    return f0 + (float3(1.0f - f0.r, 1.0f - f0.g, 1.0f - f0.b)) * pow((1.0f - cosTheta), 5.0f);
}
float3 Schlick_Fresnel_UE(float3 baseReflectivity, float cosTheta)
{
    return max(baseReflectivity + float3(1.0 - baseReflectivity.x, 1.0 - baseReflectivity.y, 1.0 - baseReflectivity.z) * pow(2, (-5.55473 * cosTheta - 6.98316) * cosTheta), 0.0);
}
float3 Schlick_Fresnel_Roughness(float cosTheta, float3 F0, float roughness)
{
    float a = 1.0f - roughness;
    return F0 + (max(float3(a, a, a), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

// ================================================================================================
float3 DirectDiffuseBRDF(float3 diffuseAlbedo, float3 lightDir, float3 viewDir, float3 F0, float metallic, float3 halfVec)
{
    float3 F = Schlick_Fresnel(F0, max(dot(halfVec, viewDir), 0.0));
    float3 kD = float3(1.0f, 1.0f, 1.0f) - F;
    kD *= (1.0f - metallic);
    
    return (diffuseAlbedo * kD) / Pi;
}

// ================================================================================================
// Cook-Torrence BRDF
float3 DirectSpecularBRDF(float3 normalWS, float3 lightDir, float3 viewDir, float roughness, float3 F0, float3 halfVec)
{
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(normalWS, halfVec, roughness);
    float G = GeometrySmith(normalWS, viewDir, lightDir, roughness);
    float3 F = Schlick_Fresnel(F0, max(dot(halfVec, viewDir), 0.0));
           
    float3 nominator = NDF * G * F;
    float denominator = 4 * max(dot(normalWS, viewDir), 0.0) * max(dot(normalWS, lightDir), 0.0);
    float3 specular = nominator / max(denominator, 0.001);
       
    return specular;
}
// ================================================================================================

float3 DirectLightingPBR(float3 normalWS, float3 lightColor, float3 diffuseAlbedo,
	float3 positionWS, float roughness, float3 F0, float metallic)
{
    float3 lightDir = SunDirection.xyz;
    float3 viewDir = normalize(CameraPosition.xyz - positionWS).rgb;
    float3 halfVec = normalize(viewDir + lightDir);
    
    float nDotL = max(dot(normalWS, lightDir), 0.0);
   
    float3 lighting = DirectDiffuseBRDF(diffuseAlbedo, lightDir, viewDir, F0, metallic, halfVec) + DirectSpecularBRDF(normalWS, lightDir, viewDir, roughness, F0, halfVec);

    float lightIntensity = SunColor.a;
    return max(lighting, 0.0f) * nDotL * lightColor * lightIntensity;
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
    
    float3 prefilteredColor = IrradianceSpecularTexture.SampleLevel(SamplerLinear, reflectDir, mipIndex);
    float2 environmentBRDF = IntegrationTexture.SampleLevel(SamplerLinear, float2(nDotV, roughness), 0).rg;

    return prefilteredColor * (F0 * environmentBRDF.x + environmentBRDF.y);

}
float3 IndirectLightingPBR(float3 diffuseAlbedo, float3 normalWS, float3 positionWS, float roughness, float3 F0, float metalness)
{
    float3 viewDir = normalize(CameraPosition.xyz - positionWS);
    float nDotV = max(dot(normalWS, viewDir), 0.0);
    float3 reflectDir = normalize(reflect(-viewDir, normalWS));
    float ao = 1.0f;

    float3 irradiance = IrradianceDiffuseTexture.SampleLevel(SamplerLinear, normalWS, 0).rgb;
    
    float3 F = Schlick_Fresnel_UE(F0, nDotV);
    float3 kD = float3(1.0f, 1.0f, 1.0f) - F;
    kD *= float3(1.0f - metalness, 1.0f - metalness, 1.0f - metalness);
    float3 indirectDiffuseLighting = kD * irradiance * diffuseAlbedo;

    float3 indirectSpecularLighting = ApproximateSpecularIBL(F, reflectDir, nDotV, roughness);
    
    return (indirectDiffuseLighting + indirectSpecularLighting) * ao;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    uint2 inPos = DTid.xy;
    
    float4 diffuseAlbedo = pow(GbufferAlbedoTexture.Load(uint3(inPos, 0)), 2.2);    
    float3 normalWS = GbufferNormalTexture.Load(uint3(inPos, 0)).rgb;
    float4 worldPos = GbufferWorldPosTexture.Load(uint3(inPos, 0));
    float4 extraGbuffer = GbufferExtraTexture.Load(uint3(inPos, 0));
    if (worldPos.a < 0.000001f)
        return;
    
    float roughness = extraGbuffer.g;
    float metalness = extraGbuffer.b;
        
    //reflectance at normal incidence for dia-electic or metal
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, diffuseAlbedo.rgb, metalness);

    float3 directLighting = float3(0.0, 0.0, 0.0);
    directLighting += DirectLightingPBR(normalWS, SunColor.xyz, diffuseAlbedo.rgb, worldPos.rgb, roughness, F0, metalness);
    
    float3 indirectLighting = float3(0.0, 0.0, 0.0);
    if (extraGbuffer.a < 1.0f)
        indirectLighting += IndirectLightingPBR(diffuseAlbedo.rgb, normalWS, worldPos.rgb, roughness, F0, metalness);
    
    float shadow = GetShadow(worldPos);
    
    float3 color = (directLighting * shadow)/* + indirectLighting*/;
    OutputTexture[inPos] += float4(color, 1.0f);
}