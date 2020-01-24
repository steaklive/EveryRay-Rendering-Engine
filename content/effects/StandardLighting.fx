// Standard Lighting Effect
static const float4 ColorWhite = { 1, 1, 1, 1 };
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
//Texture2D metallicTexture;
//Texture2D aoTexture;
Texture2D ShadowTexture;

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
    
    float3 specularAlbedo = float3(0.56, 0.56, 0.56);
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