#include "Common.hlsli"

Texture2D<float4> ColorTexture : register(t0);
Texture2D<float4> GBufferNormals : register(t1);
Texture2D<float4> GBufferExtra : register(t2); //reflection mask in R channel
Texture2D<float> DepthTexture : register(t3);

SamplerState Sampler : register(s0);

cbuffer CBufferPerObject : register(b0)
{
    float4x4 InvProjMatrix;
    float4x4 InvViewMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
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
    float4x4 projView = mul(ViewMatrix, ProjMatrix);
    bool success = false;
    
    for (int i = 1; i <= maxCount; i++)
    {
        float3 ray = (i + noise(uv + Time)) * step;
        float3 rayPos = pos + ray;
        float4 vpPos = mul(float4(rayPos, 1.0f), projView);
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
float3 ReconstructWorldPosFromDepth(float2 uv, float depth)
{
    float ndcX = uv.x * 2 - 1;
    float ndcY = 1 - uv.y * 2; // Remember to flip y!!!
    float4 viewPos = mul(float4(ndcX, ndcY, depth, 1.0f), InvProjMatrix);
    viewPos = viewPos / viewPos.w;
    return mul(viewPos, InvViewMatrix).xyz;
}

float4 PSMain(QUAD_VS_OUT IN) : SV_Target
{
    float4 color = ColorTexture.Sample(Sampler, IN.TexCoord);
    float reflectionMaskFactor = GBufferExtra.Sample(Sampler, IN.TexCoord).r;
    if (reflectionMaskFactor < 0.00001f || reflectionMaskFactor > 1.0f)
        return color;

	// we don't want reflections on foliage
    float foliageFactor = GBufferExtra.Sample(Sampler, IN.TexCoord).a;
    if (foliageFactor >= 1.0f)
        return color;
    
	float depth = DepthTexture.Sample(Sampler, IN.TexCoord).r;
    if (depth >= 1.0f) 
        return color;
    
    float4 normal = GBufferNormals.Sample(Sampler, IN.TexCoord);
    float4 worldSpacePosition = float4(ReconstructWorldPosFromDepth(IN.TexCoord, depth), 1.0f);
    float4 camDir = normalize(worldSpacePosition - CameraPosition);
    float3 refDir = normalize(reflect(normalize(camDir), normal));
    float4 reflectedColor = Raytrace(refDir, 50, StepSize, worldSpacePosition.rgb, IN.TexCoord);

    return color + reflectedColor;
}