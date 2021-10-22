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
    float WorldVoxelScale;
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

RWTexture3D<float4> outputVoxelGITexture : register(u0);

struct VS_INPUT
{
    float4 Position : POSITION;
    float2 TextureCoordinates : TEXCOORD0;
    float3 Normal : NORMAL;
    
    row_major float4x4 World : WORLD;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float2 TextureCoordinates : TEXCOORD0;
    float3 Normal : Normal;
    float3 ShadowCoord0 : TEXCOORD2;
    float3 ShadowCoord1 : TEXCOORD3;
    float3 ShadowCoord2 : TEXCOORD4;
    float3 WorldPos : WorldPos;
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

    {
        //OUT.Position = mul(localPos, IN.World);
        //OUT.Position = mul(OUT.Position, rotationMat);
        if (IN.Position.y > vertexHeight)
        {
            OUT.Position.x += sin(Time * WindFrequency + OUT.Position.x * WindGustDistance) * vertexHeight * WindStrength * WindDirection.x;
            OUT.Position.z += sin(Time * WindFrequency + OUT.Position.z * WindGustDistance) * vertexHeight * WindStrength * WindDirection.z;
        }
        OUT.WorldPos = mul(IN.Position, IN.World).xyz;
        OUT.Position = mul(OUT.Position, View);
    }

    OUT.Position = mul(OUT.Position, Projection);
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), IN.World).xyz);
    OUT.TextureCoordinates = IN.TextureCoordinates;
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

struct PS_OUTPUT_GBUFFER
{
    float4 Color : SV_Target0;
    float4 Normal : SV_Target1;
    float4 WorldPos : SV_Target2;
    float4 Extra : SV_Target3;
};

PS_OUTPUT_GBUFFER pixel_shader_gbuffer(VS_OUTPUT IN) : SV_Target
{
    PS_OUTPUT_GBUFFER OUT;

    OUT.Color = albedoTexture.Sample(ColorSampler, IN.TextureCoordinates);
    OUT.Normal = float4(IN.Normal, 1.0f);
    OUT.WorldPos = float4(IN.WorldPos, 1.0f);
    OUT.Extra = float4(0.0, 0.0, 0.0, 0.0);
    return OUT;

}

struct PS_GI_IN
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float3 VoxelPos : TEXCOORD1;
    float3 ShadowCoord0 : TEXCOORD2;
    float3 ShadowCoord1 : TEXCOORD3;
    float3 ShadowCoord2 : TEXCOORD4;
    float4 PosWVP : TEXCOORD5;
};

[maxvertexcount(3)]
void geometry_shader_voxel_gi(triangle VS_OUTPUT input[3], inout TriangleStream<PS_GI_IN> OutputStream)
{
    PS_GI_IN output[3];
    output[0] = (PS_GI_IN) 0;
    output[1] = (PS_GI_IN) 0;
    output[2] = (PS_GI_IN) 0;
    
    float3 p1 = input[1].WorldPos.rgb - input[0].WorldPos.rgb;
    float3 p2 = input[2].WorldPos.rgb - input[0].WorldPos.rgb;
    float3 n = abs(normalize(cross(p1, p2)));
       
    float axis = max(n.x, max(n.y, n.z));
    
    [unroll]
    for (uint i = 0; i < 3; i++)
    {
        output[0].VoxelPos = input[i].WorldPos.xyz / WorldVoxelScale * 2.0f;
        output[1].VoxelPos = input[i].WorldPos.xyz / WorldVoxelScale * 2.0f;
        output[2].VoxelPos = input[i].WorldPos.xyz / WorldVoxelScale * 2.0f;
        if (axis == n.z)
            output[i].Position = float4(output[i].VoxelPos.x, output[i].VoxelPos.y, 0, 1);
        else if (axis == n.x)
            output[i].Position = float4(output[i].VoxelPos.y, output[i].VoxelPos.z, 0, 1);
        else
            output[i].Position = float4(output[i].VoxelPos.x, output[i].VoxelPos.z, 0, 1);
    
        //output[i].normal = input[i].normal;
        output[i].UV = input[i].TextureCoordinates;
        output[i].ShadowCoord0 = input[i].ShadowCoord0;
        output[i].ShadowCoord1 = input[i].ShadowCoord1;
        output[i].ShadowCoord2 = input[i].ShadowCoord2;
        OutputStream.Append(output[i]);
    }
    OutputStream.RestartStrip();
}

float3 VoxelToWorld(float3 pos)
{
    float3 result = pos;
    result *= WorldVoxelScale;

    return result * 0.5f;
}

void pixel_shader_voxel_gi(PS_GI_IN input)
{
    uint width;
    uint height;
    uint depth;
    
    outputVoxelGITexture.GetDimensions(width, height, depth);
    float3 voxelPos = input.VoxelPos.rgb;
    voxelPos.y = -voxelPos.y;
    
    int3 finalVoxelPos = width * float3(0.5f * voxelPos + float3(0.5f, 0.5f, 0.5f));
    float4 colorRes = albedoTexture.Sample(ColorSampler, input.UV);
    voxelPos.y = -voxelPos.y;
    
    float4 worldPos = float4(VoxelToWorld(voxelPos), 1.0f);
    if (ShadowCascadeDistances.a < 1.0f)
        outputVoxelGITexture[finalVoxelPos] = colorRes;
    else
    {
        float shadow = GetShadow(input.ShadowCoord0, input.ShadowCoord1, input.ShadowCoord2, input.PosWVP.w);
        outputVoxelGITexture[finalVoxelPos] = colorRes * float4(shadow, shadow, shadow, 1.0f);
    }
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

technique11 to_gbuffer
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, pixel_shader_gbuffer()));

    }
}

technique11 to_voxel_gi
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(CompileShader(gs_5_0, geometry_shader_voxel_gi()));
        SetPixelShader(CompileShader(ps_5_0, pixel_shader_voxel_gi()));
    }
}