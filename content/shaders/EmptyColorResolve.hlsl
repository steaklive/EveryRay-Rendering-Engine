Texture2D<float4> finalTexture : register(t0);
SamplerState LinearSampler : register(s0);

float4 PSMain(float4 pos : SV_Position, float2 tex : TEXCOORD0) : SV_Target
{
    return finalTexture.Sample(LinearSampler, tex);
}