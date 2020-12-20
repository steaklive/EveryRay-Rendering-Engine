SamplerState SimpleSampler;

Texture2D sceneColorTex : register(t0);
Texture2D sceneDepthTex : register(t1);
Texture2D cloudsColorTex : register(t2);

float4 main(float4 pos : SV_POSITION, float2 tex : TEX_COORD0) : SV_Target
{
    float4 res;
    
    float4 sceneCol = sceneColorTex.Sample(SimpleSampler, tex);
    float sceneDepth = sceneDepthTex.Sample(SimpleSampler, tex).r;
    float4 cloudCol = cloudsColorTex.Sample(SimpleSampler, tex);

    res = lerp(sceneCol, cloudCol, ((sceneDepth < 0.9998f) ? 0.0 : 1.0));
    return res;
}