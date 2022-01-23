//Can be used for both diffuse and specular probe convolution
#define PI 3.141592654

cbuffer Constants : register (b0)
{
	int FaceIndex;
	int MipIndex;
}

TextureCube OriginalCubemap : register(t0);
SamplerState LinearSampler : register(s0);

struct VS_IN
{
	float4 Position : POSITION;
	float2 TexCoord : TEXCOORD0;
};

struct VS_OUT
{
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

VS_OUT VSMain(VS_IN input)
{
	VS_OUT output;

	output.Position = float4(input.Position.xyz, 1.0f);
	output.TexCoord = input.TexCoord;

	return output;
}

// ===============================================================================================
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
// ===============================================================================================
float2 Hammersley(uint i, uint N)
{
	float ri = reversebits(i) * 2.3283064365386963e-10f;
	return float2(float(i) / float(N), ri);
}
// ===============================================================================================
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf 
// ===============================================================================================
float3 ImportanceSampleGGX(float2 Xi, float Roughness, float3 N)
{
	float a = Roughness * Roughness; // DISNEY'S ROUGHNESS [see Burley'12 siggraph]

	float Phi = 2 * PI * Xi.x;
	float CosTheta = sqrt((1 - Xi.y) / (1 + (a * a - 1) * Xi.y));
	float SinTheta = sqrt(1 - CosTheta * CosTheta);

	float3 H;
	H.x = SinTheta * cos(Phi);
	H.y = SinTheta * sin(Phi);
	H.z = CosTheta;

	float3 UpVector = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
	float3 TangentX = normalize(cross(UpVector, N));
	float3 TangentY = cross(N, TangentX);

	// Tangent to world space
	return TangentX * H.x + TangentY * H.y + N * H.z;
}

float3 GetNormal(uint face, float2 uv)
{
	float2 debiased = uv * 2.0f - 1.0f;

	float3 dir = 0;

	switch (face)
	{
		case 0:
			dir = float3(1, -debiased.y, -debiased.x);
			break;

		case 1:
			dir = float3(-1, -debiased.y, debiased.x);
			break;

		case 2:
			dir = float3(debiased.x, 1, debiased.y);
			break;

		case 3:
			dir = float3(debiased.x, -1, -debiased.y);
			break;

		case 4:
			dir = float3(debiased.x, -debiased.y, 1);
			break;

		case 5:
			dir = float3(-debiased.x, -debiased.y, -1);
			break;
	};

	return normalize(dir);
}

// ================================================================================================
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf 
// ================================================================================================
float3 PrefilterEnvMap(float Roughness, float3 R)
{
	float TotalWeight = 0.0000001f;

	float3 N = R;
	float3 V = R;
	float3 PrefilteredColor = 0;

	const uint NumSamples = 1024;

	for (uint i = 0; i < NumSamples; i++)
	{
		float2 Xi = Hammersley(i, NumSamples);
		float3 H = ImportanceSampleGGX(Xi, Roughness, N);
		float3 L = normalize(2 * dot(V, H) * H - V);
		float NoL = saturate(dot(N, L));

		if (NoL > 0)
		{
			PrefilteredColor += OriginalCubemap.Sample(LinearSampler, L).rgb * NoL;
			TotalWeight += NoL;
		}
	}

	if (TotalWeight > 0.0f)
		return  PrefilteredColor / TotalWeight;
	else
		return PrefilteredColor;
}

float4 PSMain(VS_OUT input) : SV_TARGET
{
	float3 N = GetNormal(FaceIndex, input.TexCoord);
	if (MipIndex == -1) // diffuse
		return float4(PrefilterEnvMap(1.0, N), 1.0f);
	else // specular
		return float4(PrefilterEnvMap(saturate(MipIndex / 6.0f), N), 1.0f);
}