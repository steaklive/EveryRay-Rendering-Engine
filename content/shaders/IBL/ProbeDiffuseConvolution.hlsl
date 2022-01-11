// Simple diffuse convolution, taken from https://learnopengl.com/code_viewer_gh.php?code=src/6.pbr/2.1.2.ibl_irradiance/2.1.2.irradiance_convolution.fs
#define PI 3.141592654

cbuffer Constants : register (b0)
{
	float4 WorldPos;
	int FaceIndex;
}

TextureCube originalCubemap : register(t0);

SamplerState LinearSampler : register(s0);

struct VS_IN
{
	float4 Position 	: POSITION;
	float2 TexCoord 	: TEXCOORD0;
};

struct VS_OUT
{
	float4 Position			: SV_Position;
	float2 TexCoord			: TEXCOORD0;
};

VS_OUT VSMain(VS_IN input)
{
	VS_OUT output;

	output.Position = float4(input.Position.xyz, 1.0f);
	output.TexCoord = input.TexCoord;

	return output;
}

float4 PSMain(VS_OUT input) : SV_TARGET
{
	//return float4(1.0f, 0.0, 1.0, 1.0f);

	float3 N = normalize(WorldPos.rgb);
	float3 irradiance = float3(0.0f, 0.0f, 0.0f);
	
	float3 up = float3(0.0f, 1.0f, 0.0f);
	float3 right = normalize(cross(up, N));
	up = normalize(cross(N, right));
	
	float sampleDelta = 0.025;
	float nrSamples = 0.0;
	for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
	{
	    for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
	    {
	        // spherical to cartesian (in tangent space)
			float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
	        // tangent space to world
			float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
	
	        irradiance += originalCubemap.Sample(LinearSampler, sampleVec).rgb * cos(theta) * sin(theta);
	        nrSamples++;
	    }
	}
	irradiance = PI * irradiance * (1.0 / float(nrSamples));
	
	return float4(irradiance, 1.0);
}