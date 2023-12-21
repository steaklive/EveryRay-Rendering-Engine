#include "Common.hlsli"

Texture2D<float4> ColorTexture : register(t0);
Texture2D<float4> GBufferNormals : register(t1);
Texture2D<float4> GbufferExtraTexture : register(t2); // [extra mask value (i.e. reflection), roughness, metalness, height mask value]
Texture2D<uint> GbufferExtra2Texture : register(t3); // [rendering object bitmask flags]
Texture2D<float> DepthTexture : register(t4);

SamplerState Sampler : register(s0);

cbuffer CBufferPerObject : register(b0)
{
    float4x4 InvProjMatrix;
    float4x4 InvViewMatrix;
    float4x4 ViewProjMatrix;
	float4 CameraPosition;
    float StepSize;
    float MaxThickness;
    float Time;
    int MaxRayCount;
}

float noise(float2 seed)
{
    return frac(sin(dot(seed.xy, float2(12.9898, 78.233))) * 43758.5453);
}
float4 Raytrace(float3 reflectionWorld, const int maxCount, float stepSize, float3 pos, float2 uv)
{
    float4 color = float4(0.0, 0.0f, 0.0f, 0.0f);
    float3 step = StepSize * reflectionWorld;
    bool success = false;
    
    for (int i = 1; i <= maxCount; i++)
    {
        float3 ray = (i + noise(uv + Time)) * step;
        float3 rayPos = pos + ray;
        float4 vpPos = mul(float4(rayPos, 1.0f), ViewProjMatrix);
        float2 rayUv = vpPos.xy / vpPos.w * float2(0.5f, -0.5f) + float2(0.5f, -0.5f);

        float rayDepth = vpPos.z / vpPos.w;
        float gbufferDepth = DepthTexture.Sample(Sampler, rayUv).r;
        if (rayDepth - gbufferDepth > 0 && rayDepth - gbufferDepth < MaxThickness)
        {            
            float a = 0.3f * pow(min(1.0, (StepSize * maxCount / 2) / length(ray)), 2.0);
            color = color * (1.0f - a) + float4(ColorTexture.Sample(Sampler, rayUv).rgb, 1.0f) * a;
            //success = true;
            break;
        }
    
    }
	
    //if (!success)
    //{
    //    TODO i.e read from local cubemap
    //}
	
    return color;
}


float4 PSMain(QUAD_VS_OUT IN) : SV_Target
{
    float4 color = ColorTexture.Sample(Sampler, IN.TexCoord);
    
    uint width, height;
    GbufferExtra2Texture.GetDimensions(width, height);
    
    uint objectFlags = GbufferExtra2Texture.Load(uint3(IN.TexCoord * uint2(width, height), 0)).r;
    if (!(objectFlags & RENDERING_OBJECT_FLAG_REFLECTION) ||
        (objectFlags & RENDERING_OBJECT_FLAG_FOLIAGE) || (objectFlags & RENDERING_OBJECT_FLAG_TRANSPARENT) || (objectFlags & RENDERING_OBJECT_FLAG_FUR))
        return color;

    float reflectionMaskFactor = GbufferExtraTexture.Sample(Sampler, IN.TexCoord).r;
    if (reflectionMaskFactor < 0.0001)
        return color;
    
	float depth = DepthTexture.Sample(Sampler, IN.TexCoord).r;
    if (depth >= 1.0f) 
        return color;
    
    float4 normal = GBufferNormals.Sample(Sampler, IN.TexCoord);
    float4 worldSpacePosition = float4(ReconstructWorldPosFromDepth(IN.TexCoord, depth, InvProjMatrix, InvViewMatrix), 1.0f);
    float4 camDir = normalize(worldSpacePosition - CameraPosition);
    float3 refDir = normalize(reflect(normalize(camDir), normal));
    float4 reflectedColor = Raytrace(refDir, 50, StepSize, worldSpacePosition.rgb, IN.TexCoord);

    return color + reflectedColor;
}