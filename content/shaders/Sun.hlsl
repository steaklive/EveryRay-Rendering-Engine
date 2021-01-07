Texture2D skyTex : register(t0);
Texture2D sceneColorTex : register(t1);
Texture2D sceneDepthTex : register(t2);

SamplerState DefaultSampler : register(s0);

static const float4 SUN_COLOR = float4(0.92f, 0.7f, 0.294f, 1.0f);

cbuffer Params : register (b0)
{
    float4x4 InvProj;
    float4x4 InvView;
    
    float4 SunDir;
    float4 SunColor;
    float SunExponent;
    float SunBrightness;
};


float3 toClipSpaceCoord(float2 tex)
{
    float2 ray;
    ray.x = 2.0 * tex.x - 1.0;
    ray.y = 1.0 - tex.y * 2.0;
    
    return float3(ray, 1.0);
}

float4 renderSun(float3 worldDir, bool occlusion = false)
{
    float sun = clamp(dot(-SunDir.rgb, worldDir), 0.0f, 1.0f);
    if (occlusion)
        return SunBrightness * pow(sun, SunExponent);
    else
        return SunBrightness * /*SunColor*/SUN_COLOR * pow(sun, SunExponent);
}

float4 main(float4 pos : SV_POSITION, float2 tex : TEX_COORD0) : SV_Target
{
    //compute ray direction
    float4 rayClipSpace = float4(toClipSpaceCoord(tex), 1.0);
    float4 rayView = mul(InvProj, rayClipSpace);
    rayView = float4(rayView.xy, -1.0, 0.0);
    
    float3 worldDir = mul(InvView, rayView).xyz;
    worldDir = normalize(worldDir);
    
    float4 sunColor = skyTex.Sample(DefaultSampler, tex);
    sunColor += renderSun(worldDir);
    
    float4 sceneCol = sceneColorTex.Sample(DefaultSampler, tex);
    float sceneDepth = sceneDepthTex.Sample(DefaultSampler, tex).r;
    
    return lerp(sceneCol, sunColor, ((sceneDepth < 0.9998f) ? 0.0 : 1.0));
}

// for light shafts
float4 occlusion(float4 pos : SV_POSITION, float2 tex : TEX_COORD0) : SV_Target
{
    //compute ray direction
    float4 rayClipSpace = float4(toClipSpaceCoord(tex), 1.0);
    float4 rayView = mul(InvProj, rayClipSpace);
    rayView = float4(rayView.xy, -1.0, 0.0);
    
    float3 worldDir = mul(InvView, rayView).xyz;
    worldDir = normalize(worldDir);
    
    float4 color = float4(0.1, 0.1, 0.1, 1.0);
    color += renderSun(worldDir, true);
    float depth = sceneDepthTex.Sample(DefaultSampler, tex).r;
    color = lerp(float4(0.1, 0.1, 0.1, 1.0), color, (depth < 0.9998f) ? 0.0f : 1.0f);
    
    if (depth < 0.9998f)
        color = float4(0.0, 0.0, 0.0, 1.0);
    
    return color;
}