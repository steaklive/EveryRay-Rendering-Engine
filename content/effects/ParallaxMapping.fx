// Parallax Occlusion Mapping with PBR, IBL (no specular IBL), AO and Soft Shadows



static const float PI = 3.14159265359f;
static const float NORMAL_FROM_HEIGHT_SCALE = 50.0f;
static const float HEIGHTMAP_RESOLUTION = 512.0f; //maybe should be loaded from cbuffer
static const float TILING = 10.0f; //maybe should be loaded from cbuffer

cbuffer ObjectData : register(b0)
{
    float4x4 ViewProjection;
    float4x4 World;
    float4 CameraPosition;
    float4 Albedo;
    float Metalness;
    float Roughness;
    
    float UseAmbientOcclusion; //for debugging: 1.0f - enabled, 0.0f - disabled 
    float pad;
}

cbuffer LightData : register(b1)
{
    float4 LightPosition;
    float4 LightColor;
};

cbuffer OtherData : register(b2)
{
    float POMHeightScale;
    float POMSoftShadows; //for debugging: 1.0f - enabled, 0.0f - disabled
    float DemonstrateAO; //for debugging: 1.0f - enabled, 0.0f - disabled
    float DemonstrateNormal; //for debugging: 1.0f - enabled, 0.0f - disabled
};

SamplerState AnisotropicSampler
{
    Filter = ANISOTROPIC;
    AddressU = Wrap;
    AddressV = Wrap;
};
SamplerState BilinearSampler
{
    Filter = MIN_MAG_LINEAR_MIP_POINT;
    AddressU = Wrap;
    AddressV = Wrap;
};
SamplerState LinearSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

Texture2D HeightMap;
TextureCube EnvMap;

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
    float3 Normal : Normal;
    float3 Tangent : Tangent;
    float3 Binormal : Binormal;
    
    float3 TangentLightDir : TangentLightDir;
    float3 TangentViewDir : TangentViewDir;
    float2 ParallaxOffset : ParallaxOffset;
    float2 ParallaxOffsetSelfShadow : ParallaxOffsetSelfShadow;
};

// https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/chapter5-andersson-terrain-rendering-in-frostbite.pdf
float3 GetNormalFromHeightmap(float2 uv, float texelSize, float maxHeight)
{
    float4 h;
    h[0] = HeightMap.SampleLevel(BilinearSampler, uv + texelSize * float2(0, -1), 0).r * maxHeight;
    h[1] = HeightMap.SampleLevel(BilinearSampler, uv + texelSize * float2(-1, 0), 0).r * maxHeight;
    h[2] = HeightMap.SampleLevel(BilinearSampler, uv + texelSize * float2(1, 0), 0).r * maxHeight;
    h[3] = HeightMap.SampleLevel(BilinearSampler, uv + texelSize * float2(0, 1), 0).r * maxHeight;
    
    float3 n;
    n.z = h[0] - h[3];
    n.x = h[1] - h[2];
    n.y = 2;

    return normalize(n);
}

float CalculateParallaxOcculstionMappingSelfShadow(float2 parallaxOffset, float2 uv, int numSteps)
{
    float2 dx = ddx(uv);
    float2 dy = ddy(uv);
          
    float currentHeight = 0.0f;
    float stepSize = 1.0f / (float) numSteps;
    int stepIndex = 0;

    float2 texOffsetPerStep = stepSize * parallaxOffset;
    float2 texCurrentOffset = uv;
    float currentBound = HeightMap.SampleGrad(LinearSampler, texCurrentOffset, dx, dy).r;
    float softShadow = 0.0f;
    
    while (stepIndex < numSteps)
    {
        texCurrentOffset += texOffsetPerStep;
        currentHeight = HeightMap.SampleGrad(LinearSampler, texCurrentOffset, dx, dy).r;

        currentBound += stepSize;

        if (currentHeight > currentBound)
        {
            if (POMSoftShadows == 1.0f)
            {
                float newSoftShadow = (currentHeight - currentBound) * (1.0 - (float) stepIndex / (float) numSteps);
                softShadow = max(softShadow, newSoftShadow);
            }
            else 
                stepIndex = numSteps + 1;
        }
        else
            stepIndex++;
    }
    
    float shadow = 0.0f;
    float softShadowFactor = 3.0f;
    
    if (POMSoftShadows == 1.0f)
        shadow = (stepIndex >= numSteps) ? (1.0f - clamp(softShadowFactor * softShadow, 0.0f, 1.0f)) : 1.0f;
    else
        shadow = (stepIndex > numSteps) ? 0.0f : 1.0f;
    
    return shadow;
}

float2 CalculateParallaxOcclusionMappingUVOffset(float2 parallaxOffset, float2 uv, int numSteps)
{
    float currentHeight = 0.0f;
    float stepSize = 1.0f / (float) numSteps;
    float prevHeight = 1.0f;
    float nextHeight = 0.0f;
    int stepIndex = 0;

    float2 texOffsetPerStep = stepSize * parallaxOffset;
    float2 texCurrentOffset = uv;
    float currentBound = 1.0;
    float parallaxAmount = 0.0;

    float2 pt1 = 0;
    float2 pt2 = 0;

    float2 texOffset2 = 0;
    float2 dx = ddx(uv);
    float2 dy = ddy(uv);
    
    while (stepIndex < numSteps)
    {
        texCurrentOffset -= texOffsetPerStep;
        currentHeight = HeightMap.SampleGrad(LinearSampler, texCurrentOffset, dx, dy).r;

        currentBound -= stepSize;

        if (currentHeight > currentBound)
        {
            pt1 = float2(currentBound, currentHeight); // point from current height
            pt2 = float2(currentBound + stepSize, prevHeight); // point from previous height

            texOffset2 = texCurrentOffset - texOffsetPerStep;

            stepIndex = numSteps + 1;
        }
        else
        {
            stepIndex++;
            prevHeight = currentHeight;
        }
    }
   
    //linear interpolation
    float delta2 = pt2.x - pt2.y;
    float delta1 = pt1.x - pt1.y;
    float diff = delta2 - delta1;
      
    if (diff == 0.0f)
        parallaxAmount = 0.0f;
    else
        parallaxAmount = (pt1.x * delta2 - pt2.x * delta1) / diff;
   
    float2 vParallaxOffset = parallaxOffset * (1.0 - parallaxAmount);
    return uv - vParallaxOffset;
}

// dynamic LOD for num of parallax ray layers
int GetRayStepsCount(float3 worldPos, float3 normal)
{
    int minLayers = 1;
    int maxLayers = 32;
    
    // this optimization is disabled here because camera is kinda "static" in requirements
    int numLayers = maxLayers; //   (int) lerp(maxLayers, minLayers, dot(normalize(CameraPosition.rgb - worldPos), normal));
    return numLayers;
}

// http://graphicrants.blogspot.com.au/2013/08/specular-brdf-reference.html
float GGX(float NdotV, float a)
{
    float k = a / 2.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

// http://blog.selfshadow.com/publications/s2013-shading-course/rad/s2013_pbs_rad_notes.pdf
// http://graphicrants.blogspot.fr/2013/08/specular-brdf-reference.html
float G_Smith(float a, float nDotV, float nDotL)
{
    return GGX(nDotL, a * a) * GGX(nDotV, a * a);
}

// http://blog.selfshadow.com/publications/s2013-shading-course/rad/s2013_pbs_rad_notes.pdf
// http://graphicrants.blogspot.fr/2013/08/specular-brdf-reference.html
float3 Schlick_Fresnel(float3 f0, float3 h, float3 l)
{
    return f0 + (1.0f - f0) * pow((1.0f - dot(l, h)), 5.0f);
}

float3 CalculateFresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0f);
}

float3 CalculateIndirectLightingPBR(float3 diffuseAlbedo, float3 specularAlbedo, float3 lightDir, float3 positionWS, float3 normalWS, float metalness, float roughness)
{
    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, diffuseAlbedo, metalness);
    
    float3 viewDir = normalize(CameraPosition.rgb - positionWS);
    //float3 halfVec = normalize(viewDir + lightDir);
    float3 reflectDir = normalize(reflect(-viewDir, normalWS));
    float nDotV = max(dot(normalWS, viewDir), 0.0001f);
    
    float3 F = CalculateFresnelSchlickRoughness(nDotV, F0, roughness);
    
    // Not a physically correct way to sample irradiance - just load a mipped environment map...
    // Irradiance should be either pre-loaded or precomputed with convolution
    float mip = 8.0 * roughness;
    float3 irradiance = EnvMap.SampleLevel(LinearSampler, normalWS, mip).rgb;
    
    float3 kd = lerp(float3(1.0f, 1.0f, 1.0f) - F, float3(0.0f, 0.0f, 0.0f), metalness);
    float3 diffuseIBL = kd * diffuseAlbedo * irradiance;
    
	// no specularIBL as we dont have specular irradiance texture and BRDF lut
    // specularIBL = ...
    return diffuseIBL /*+ specularIBL*/;
}

// Lambertian BRDF
float3 DirectDiffuseBRDF(float3 diffuseAlbedo, float nDotL)
{
    return (diffuseAlbedo * nDotL) / PI;
}
// Cook-Torrence BRDF
float3 DirectSpecularBRDF(float3 specularAlbedo, float3 positionWS, float3 normalWS, float3 lightDir, float roughness)
{
    float3 viewDir = normalize(CameraPosition.rgb - positionWS);
    float3 halfVec = normalize(viewDir + lightDir);

    float nDotH = clamp(dot(normalWS, halfVec), 0.0, 1.0);
    float nDotL = clamp(dot(normalWS, lightDir), 0.0, 1.0);
    float nDotV = max(dot(normalWS, viewDir), 0.0001f);

    float alpha2 = roughness * roughness;

	// Normal distribution term with Trowbridge-Reitz/GGX.
    float D = alpha2 / (PI * pow(nDotH * nDotH * (alpha2 - 1.0) + 1.0, 2.0f));
 
	// Fresnel term with Schlick's approximation.
    float3 F = Schlick_Fresnel(specularAlbedo, halfVec, lightDir);

	// Geometry term with Smith's approximation.
    float G = G_Smith(roughness, nDotV, nDotL);

    return D * F * G;
}

float3 CalculateDirectLightingPBR(float3 normalWS, float3 lightDir, float3 lightColor, float3 diffuseAlbedo, float3 specularAlbedo, float3 positionWS, float roughness)
{
    float nDotL = clamp(dot(normalWS, lightDir), 0.0, 1.0);

    float3 lighting = float3(0.0f, 0.0f, 0.0f);
    if (nDotL > 0.0f)
        lighting = DirectDiffuseBRDF(diffuseAlbedo, nDotL) + DirectSpecularBRDF(specularAlbedo, positionWS, normalWS, lightDir, roughness);

    return max(lighting, 0.0f) * lightColor;
}

//https://github.com/Jam3/glsl-fast-gaussian-blur
float3 HeightMapBlur5(float2 uv, float2 resolution, float2 direction)
{
    float3 color = float3(0.0f, 0.0f, 0.0f);
    float2 off1 = float2(1.3333f, 1.3333f) * direction;
    color += HeightMap.Sample(LinearSampler, uv) * 0.29411764705882354f;
    color += HeightMap.Sample(LinearSampler, uv + (off1 / resolution)) * 0.35294117647058826f;
    color += HeightMap.Sample(LinearSampler, uv - (off1 / resolution)) * 0.35294117647058826f;
    return color;
}

// calculating an average of heightmap sampled value with gaussian for ao approximation
// ... maybe should be precomputed in a different pass with a compute shader
float CalculateAOFromHeightmap(float2 uv)
{
    float radius = 30.0f;
    float resV = HeightMapBlur5(uv, float2(HEIGHTMAP_RESOLUTION, HEIGHTMAP_RESOLUTION), float2(0.0f, radius));
    float resH = HeightMapBlur5(uv, float2(HEIGHTMAP_RESOLUTION, HEIGHTMAP_RESOLUTION), float2(radius, 0.0f));
    
    float res = 0.0f;
    for (int i = 0; i < 8; i++)
        res += (i % 2) ? resV : resH;
    
    res /= 8;
        
    return 1.0f + res - HeightMap.Sample(LinearSampler, uv).r;
}

VS_OUTPUT mainVS(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;

    OUT.Position = mul(IN.Position, mul(World, ViewProjection));
    OUT.WorldPos = mul(IN.Position, World).xyz;
    OUT.UV = IN.Texcoord0 * TILING;
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), World).xyz);
    OUT.Tangent = normalize(mul(float4(IN.Tangent, 0), World).xyz);
    OUT.Binormal = cross(OUT.Normal, OUT.Tangent);
    float3x3 TBN = float3x3(OUT.Tangent, OUT.Binormal, OUT.Normal);
    
    OUT.TangentViewDir = CameraPosition.rgb - OUT.WorldPos.rgb;
    OUT.TangentViewDir = mul(TBN, OUT.TangentViewDir); //view dir in tangent space
    
    float3 vLightWS = LightPosition.rgb - OUT.WorldPos.rgb;
    OUT.TangentLightDir = mul(TBN, vLightWS); //light dir in tangent space
    
    // calculating parallax offsets in vertex shader as optimization
    float2 parallaxDir = normalize(OUT.TangentViewDir.xy);
    float viewDirTSLength = length(OUT.TangentViewDir);
    float parallaxLength = sqrt(viewDirTSLength * viewDirTSLength - OUT.TangentViewDir.z * OUT.TangentViewDir.z) / OUT.TangentViewDir.z;
    OUT.ParallaxOffset = parallaxDir * parallaxLength * POMHeightScale;
    
    // offset for self-shadowing using light direction in tangent space
    float2 parallaxLightDir = normalize(OUT.TangentLightDir.xy);
    float parallaxLightDirTSLength = length(OUT.TangentLightDir);
    float parallaxLightDirLength = sqrt(parallaxLightDirTSLength * parallaxLightDirTSLength - OUT.TangentLightDir.z * OUT.TangentLightDir.z) / OUT.TangentLightDir.z;
    OUT.ParallaxOffsetSelfShadow = parallaxLightDir * parallaxLightDirLength * POMHeightScale;
    
    return OUT;
}

float3 mainPS(VS_OUTPUT IN) : SV_Target0
{
    float2 texCoord = IN.UV;
    int parallaxOcclusionSteps = GetRayStepsCount(IN.WorldPos.rgb, IN.Normal);
    
    texCoord = CalculateParallaxOcclusionMappingUVOffset(IN.ParallaxOffset, IN.UV, parallaxOcclusionSteps);

    float3 lightDirectionWS = normalize(LightPosition.rgb - IN.WorldPos.rgb);
    float3 lightDirectionTS = normalize(IN.TangentLightDir);
    
    float ambientOcclussion = (UseAmbientOcclusion == 1.0f) ? CalculateAOFromHeightmap(texCoord) : 1.0f;
    if (DemonstrateAO)
        return ambientOcclussion;
    
    float3 normalWS = GetNormalFromHeightmap(texCoord, 1.0f / HEIGHTMAP_RESOLUTION, NORMAL_FROM_HEIGHT_SCALE);
    if (DemonstrateNormal)
        return normalWS;
    
    float3 specularAlbedo = float3(Metalness, Metalness, Metalness);

    float dc = max(dot(normalWS, lightDirectionWS), 0.0f);
    float selfShadow = 0.0f;
    if (dc > 0.0f)
        selfShadow = CalculateParallaxOcculstionMappingSelfShadow(IN.ParallaxOffsetSelfShadow, texCoord, parallaxOcclusionSteps);

    float3 directLighting = CalculateDirectLightingPBR(normalWS, lightDirectionWS, LightColor.rgb, Albedo.rgb, specularAlbedo, IN.WorldPos.rgb, Roughness);
    float3 indirectLighting = CalculateIndirectLightingPBR(Albedo.rgb, specularAlbedo, lightDirectionWS, IN.WorldPos.rgb, normalWS, Metalness, Roughness);
    
    float3 ambient = float3(0.3, 0.3, 0.3) * Albedo.rgb * ambientOcclussion;
    return ambient + (directLighting + indirectLighting) * selfShadow;
}

technique11 parallax
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, mainVS()));
        SetPixelShader(CompileShader(ps_5_0, mainPS()));
    }
}