SamplerState SimpleSampler;

Texture2D sceneDepthTex : register(t0);
Texture2D cloudsColorTex : register(t1);

float4 main(float4 pos : SV_Position, float2 tex : TEXCOORD0) : SV_Target
{
    float4 res;
    
    float sceneDepth = sceneDepthTex.Sample(SimpleSampler, tex).r;
    float4 cloudCol = cloudsColorTex.Sample(SimpleSampler, tex);

    if (sceneDepth < 0.9998f)
        discard;
    
    return cloudCol;
}