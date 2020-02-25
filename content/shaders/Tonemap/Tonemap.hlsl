#include "TonemapShaderConstants.h"

Texture2D g_Input0 : register( t0 );
Texture2D g_Input1 : register(t1);
Texture2D g_ToneMapScale : register(t2);
SamplerState samLinear : register( s0 );


cbuffer PostCBuffer : register(b0)
{
	uint mipLevel0 = 0; 
	uint mipLevel1 = 1; 
	uint empty0;
	uint empty1;

};

cbuffer BloomConstants : register(b1)
{
	float middleGrey = MIDDLE_GREY;
	float bloomThreshold = BLOOM_THRESHOLD;
	float bloomMultiplier = BLOOM_MULTIPLIER;
	float empty2;
};

float CalcLuminance( float4 pos : SV_POSITION, float2 tex : TEX_COORD0) : SV_Target 
{     
	return log(dot(g_Input0.Sample(samLinear, tex).rgb, LUM_WEIGHTS)+DELTA); 
} 

float AvgLuminance( ) : SV_Target 
{     
	return  middleGrey/(exp(g_Input0.SampleLevel(samLinear, float2(0.5, 0.5), 9).r)-DELTA);
}

float4 Bright(float4 pos : SV_POSITION, float2 tex : TEX_COORD0 ) : SV_Target  
{
	return max(g_Input0.SampleLevel(samLinear, tex, 0) - bloomThreshold, (float4)0.0f);
}

float4 Add( float4 pos : SV_POSITION, float2 tex : TEX_COORD0 ) : SV_Target
{
	return 0.5f * (g_Input0.SampleLevel(samLinear, tex, mipLevel0) + g_Input1.SampleLevel(samLinear, tex, mipLevel1)); 
}
float4 BlurHorizontal(float4 pos : SV_POSITION) : SV_TARGET
{
    const float2 OFFSETS[] = {float2(-4, 0), float2(-3, 0), float2(-2, 0), float2(-1, 0), float2(0, 0), float2(1, 0), float2(2, 0), float2(3, 0), float2(4, 0)};
    static const float g_BlurWeights[] =
    {
        0.004815026f,
		0.028716039f,
		0.102818575f,
		0.221024189f,
		0.28525234f,
		0.221024189f,
		0.102818575f,
		0.028716039f,
		0.004815026f
    };

    float4
	output = g_Input0.Load(float3(pos.xy + OFFSETS[0], mipLevel0)) * g_BlurWeights[0];
    output += g_Input0.Load(float3(pos.xy + OFFSETS[1], mipLevel0)) * g_BlurWeights[1];
    output += g_Input0.Load(float3(pos.xy + OFFSETS[2], mipLevel0)) * g_BlurWeights[2];
    output += g_Input0.Load(float3(pos.xy + OFFSETS[3], mipLevel0)) * g_BlurWeights[3];
    output += g_Input0.Load(float3(pos.xy + OFFSETS[4], mipLevel0)) * g_BlurWeights[4];
    output += g_Input0.Load(float3(pos.xy + OFFSETS[5], mipLevel0)) * g_BlurWeights[5];
    output += g_Input0.Load(float3(pos.xy + OFFSETS[6], mipLevel0)) * g_BlurWeights[6];
    output += g_Input0.Load(float3(pos.xy + OFFSETS[7], mipLevel0)) * g_BlurWeights[7];
    output += g_Input0.Load(float3(pos.xy + OFFSETS[8], mipLevel0)) * g_BlurWeights[8];
	
    return output;
}
float4 BlurVertical(float4 pos : SV_POSITION) : SV_TARGET
{
	const float2 OFFSETS[] = {float2(0, -4), float2(0, -3), float2(0, -2), float2(0, -1), float2(0, 0), float2(0, 1), float2(0, 2), float2(0, 3), float2(0, 4)};

	static const float g_BlurWeights[] = {
		0.004815026f,
		0.028716039f,
		0.102818575f,
		0.221024189f,
		0.28525234f,
		0.221024189f,
		0.102818575f,
		0.028716039f,
		0.004815026f
	};

	float4 
	output  = g_Input0.Load(float3(pos.xy + OFFSETS[0], mipLevel0)) * g_BlurWeights[0];
	output += g_Input0.Load(float3(pos.xy + OFFSETS[1], mipLevel0)) * g_BlurWeights[1];
	output += g_Input0.Load(float3(pos.xy + OFFSETS[2], mipLevel0)) * g_BlurWeights[2];
	output += g_Input0.Load(float3(pos.xy + OFFSETS[3], mipLevel0)) * g_BlurWeights[3];
	output += g_Input0.Load(float3(pos.xy + OFFSETS[4], mipLevel0)) * g_BlurWeights[4];
	output += g_Input0.Load(float3(pos.xy + OFFSETS[5], mipLevel0)) * g_BlurWeights[5];
	output += g_Input0.Load(float3(pos.xy + OFFSETS[6], mipLevel0)) * g_BlurWeights[6];
	output += g_Input0.Load(float3(pos.xy + OFFSETS[7], mipLevel0)) * g_BlurWeights[7];
	output += g_Input0.Load(float3(pos.xy + OFFSETS[8], mipLevel0)) * g_BlurWeights[8];
	
	return output; 
}


float4 ToneMapWithBloom(float4 pos: SV_POSITION, float2 tex : TEX_COORD0) : SV_Target
{
	float3 Lw = g_Input0.Load(float3(pos.xy, 0), 0).xyz;
    Lw += bloomMultiplier * g_Input1.SampleLevel(samLinear, tex, 0).xyz;
	float scale = g_ToneMapScale.Load(float3(0, 0, 0)).r;
	float3 L = scale * Lw; 
	float3 Ld = (L/(1+L)); 
	return float4(Ld, 1.0f); 
}

float4 EmptyPass(float4 pos : SV_POSITION, float2 tex : TEX_COORD0) : SV_Target
{
    return g_Input0.Sample(samLinear, tex).rgba;
}
