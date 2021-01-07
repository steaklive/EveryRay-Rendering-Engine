static const float PI = 3.141592654f;
#define NUM_OF_SHADOW_CASCADES 3
static const float4 ColorWhite = { 1, 1, 1, 1 };

cbuffer CBufferPerObject
{
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
    float4x4 ShadowMatrices[NUM_OF_SHADOW_CASCADES];
}

cbuffer CBufferPerFrame
{
    float4 SunDirection;
    float4 SunColor;
    float4 AmbientColor;
    float4 ShadowTexelSize;
    float4 ShadowCascadeDistances;
    float4 CameraDirection;
    float RotateToCamera;
    float Time;
    float WindFrequency;
    float WindStrength;
    float WindGustDistance;
    float4 WindDirection;
}

SamplerState ColorSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = WRAP;
    AddressV = WRAP;
    AddressW = WRAP;
};

SamplerComparisonState CascadedPcfShadowMapSampler
{
    Filter = COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    AddressU = BORDER;
    AddressV = BORDER;
    BorderColor = ColorWhite;
    ComparisonFunc = LESS_EQUAL;
};

Texture2D albedoTexture;
Texture2D cascadedShadowTextures[NUM_OF_SHADOW_CASCADES];

struct VS_INPUT
{
    float4 Position : POSITION;
    float2 TextureCoordinates : TEXCOORD0;
    row_major float4x4 World : WORLD;
    float3 Color : TEXCOORD1;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float2 TextureCoordinates : TEXCOORD0;
    float3 Color : TEXCOORD1;
    float3 ShadowCoord0 : TEXCOORD2;
    float3 ShadowCoord1 : TEXCOORD3;
    float3 ShadowCoord2 : TEXCOORD4;
};

VS_OUTPUT vertex_shader(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;
    
    float4x4 scaleMat;
    float scaleX = sqrt(IN.World[0][0] * IN.World[0][0] + IN.World[0][1] * IN.World[0][1] + IN.World[0][2] * IN.World[0][2]);
    float scaleY = sqrt(IN.World[1][0] * IN.World[1][0] + IN.World[1][1] * IN.World[1][1] + IN.World[1][2] * IN.World[1][2]);
    float scaleZ = sqrt(IN.World[2][0] * IN.World[2][0] + IN.World[2][1] * IN.World[2][1] + IN.World[2][2] * IN.World[2][2]);
    
    scaleMat[0][0] = scaleX;
    scaleMat[0][1] = 0.0f;
    scaleMat[0][2] = 0.0f;
    scaleMat[0][3] = 0.0f;
    scaleMat[1][0] = 0.0f;
    scaleMat[1][1] = scaleY;
    scaleMat[1][2] = 0.0f;
    scaleMat[1][3] = 0.0f;
    scaleMat[2][0] = 0.0f;
    scaleMat[2][1] = 0.0f;
    scaleMat[2][2] = scaleZ;
    scaleMat[2][3] = 0.0f;
    scaleMat[3][0] = 0.0f;
    scaleMat[3][1] = 0.0f;
    scaleMat[3][2] = 0.0f;
    scaleMat[3][3] = 1.0f;
    
    float4x4 translateMat;
    translateMat[0][0] = 1.0f;
    translateMat[0][1] = 0.0f;
    translateMat[0][2] = 0.0f;
    translateMat[0][3] = 0.0f;
    translateMat[1][0] = 0.0f;
    translateMat[1][1] = 1.0f;
    translateMat[1][2] = 0.0f;
    translateMat[1][3] = 0.0f;
    translateMat[2][0] = 0.0f;
    translateMat[2][1] = 0.0f;
    translateMat[2][2] = 1.0f;
    translateMat[2][3] = 0.0f;
    translateMat[3][0] = IN.World[3][0];
    translateMat[3][1] = IN.World[3][1];
    translateMat[3][2] = IN.World[3][2];
    translateMat[3][3] = IN.World[3][3];
    
    float4x4 rotationMat;
    rotationMat[0][0] = IN.World[0][0] / scaleX;
    rotationMat[0][1] = IN.World[0][1] / scaleX;
    rotationMat[0][2] = IN.World[0][2] / scaleX;
    rotationMat[0][3] = 0.0f;
    rotationMat[1][0] = IN.World[1][0] / scaleY;
    rotationMat[1][1] = IN.World[1][1] / scaleY;
    rotationMat[1][2] = IN.World[1][2] / scaleY;
    rotationMat[1][3] = 0.0f;
    rotationMat[2][0] = IN.World[2][0] / scaleZ;
    rotationMat[2][1] = IN.World[2][1] / scaleZ;
    rotationMat[2][2] = IN.World[2][2] / scaleZ;
    rotationMat[2][3] = 0.0f;
    rotationMat[3][0] = 0.0f;
    rotationMat[3][1] = 0.0f;
    rotationMat[3][2] = 0.0f;
    rotationMat[3][3] = 1.0f;
    
    float4 localPos = IN.Position;
    float vertexHeight = 0.5f;
    
    float angle = atan2(CameraDirection.x, CameraDirection.z) * (180.0 / PI);
    angle *= 0.0174532925f;
    
    float4x4 newRotationMat;
    newRotationMat[0][0] = cos(angle);
    newRotationMat[0][1] = 0.0f;
    newRotationMat[0][2] = -sin(angle);
    newRotationMat[0][3] = 0.0f;
    newRotationMat[1][0] = 0.0f;
    newRotationMat[1][1] = 1.0f;
    newRotationMat[1][2] = 0.0f;
    newRotationMat[1][3] = 0.0f;
    newRotationMat[2][0] = sin(angle);
    newRotationMat[2][1] = 0.0f;
    newRotationMat[2][2] = cos(angle);
    newRotationMat[2][3] = 0.0f;
    newRotationMat[3][0] = 0.0f;
    newRotationMat[3][1] = 0.0f;
    newRotationMat[3][2] = 0.0f;
    newRotationMat[3][3] = 1.0f;
    
    localPos = mul(localPos, scaleMat);
    if (RotateToCamera > 0.0f)
        localPos = mul(localPos, newRotationMat);
    localPos = mul(localPos, translateMat);
    OUT.Position = localPos;

    //if (RotateToCamera > 0.0f)
    //{
    //    float scaleX = sqrt(IN.World[0][0] * IN.World[0][0] + IN.World[0][1] * IN.World[0][1] + IN.World[0][2] * IN.World[0][2]);
    //    //float scaleY = sqrt(IN.World[1][0] * IN.World[1][0] + IN.World[1][1] * IN.World[1][1] + IN.World[1][2] * IN.World[1][2]);
    //    //float scaleZ = sqrt(IN.World[2][0] * IN.World[2][0] + IN.World[2][1] * IN.World[2][1] + IN.World[2][2] * IN.World[2][2]);
    //
    //    float4x4 modelView = mul(IN.World, View);
    //
    //    //cylindrical rotation to camera trick from https://www.geeks3d.com/20140807/billboarding-vertex-shader-glsl/
    //    modelView[0][0] = scaleX;
    //    modelView[0][1] = 0.0f;
    //    modelView[0][2] = 0.0f;
    //
    //    //modelView[1][0] = 0.0f; //uncomment for spherical rotation
    //    //modelView[1][1] = 1.0f; //uncomment for spherical rotation
    //    //modelView[1][2] = 0.0f; //uncomment for spherical rotation
    //
    //    modelView[2][0] = 0.0f;
    //    modelView[2][1] = 0.0f;
    //    modelView[2][2] = 1.0f; //dont care bout z scale in a billboard
    //
    //    OUT.Position = mul(IN.Position, modelView);
    //}
    //else
    {
        //OUT.Position = mul(localPos, IN.World);
        //OUT.Position = mul(OUT.Position, rotationMat);
        if (IN.Position.y > vertexHeight)
        {
            OUT.Position.x += sin(Time * WindFrequency + OUT.Position.x * WindGustDistance) * vertexHeight * WindStrength * WindDirection.x;
            OUT.Position.z += sin(Time * WindFrequency + OUT.Position.z * WindGustDistance) * vertexHeight * WindStrength * WindDirection.z;
        }
        OUT.Position = mul(OUT.Position, View);
    }

    OUT.Position = mul(OUT.Position, Projection);
    OUT.TextureCoordinates = IN.TextureCoordinates;
    OUT.Color = IN.Color;
    OUT.ShadowCoord0 = mul(IN.Position, mul(IN.World, ShadowMatrices[0])).xyz;
    OUT.ShadowCoord1 = mul(IN.Position, mul(IN.World, ShadowMatrices[1])).xyz;
    OUT.ShadowCoord2 = mul(IN.Position, mul(IN.World, ShadowMatrices[2])).xyz;
    
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
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy, ShadowCoord.z) +
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d2, d1), ShadowCoord.z) +
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d1, -d2), ShadowCoord.z) +
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d2, -d1), ShadowCoord.z) +
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d1, d2), ShadowCoord.z) +
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d4, d3), ShadowCoord.z) +
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(-d3, -d4), ShadowCoord.z) +
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d4, -d3), ShadowCoord.z) +
        cascadedShadowTextures[index].SampleCmpLevelZero(CascadedPcfShadowMapSampler, ShadowCoord.xy + float2(d3, d4), ShadowCoord.z)
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

float4 pixel_shader(VS_OUTPUT IN) : SV_Target
{
    float4 albedo = albedoTexture.Sample(ColorSampler, IN.TextureCoordinates);
    float shadow = GetShadow(IN.ShadowCoord0, IN.ShadowCoord1, IN.ShadowCoord2, IN.Position.w);
    shadow = clamp(shadow, 0.5f, 1.0f);
    
    float4 color = albedo * SunColor * float4(shadow, shadow, shadow, 1.0f);
    color = saturate(color);

    return color;
}

/************* Techniques *************/
technique11 main
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, pixel_shader()));

    }
}