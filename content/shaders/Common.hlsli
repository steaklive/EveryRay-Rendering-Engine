#define NUM_OF_SHADOW_CASCADES 3

static const float PI = 3.141592654f;

cbuffer ObjectCBuffer : register(b1)
{
    float4x4 World;
    float UseGlobalProbe;
    float SkipIndirectProbeLighting;
    float IndexOfRefraction;
    float CustomRoughness;
    float CustomMetalness;
    float OriginalInstanceCount;
    float IsIndirectlyRendered;
};

struct QUAD_VS_IN
{
    float4 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct QUAD_VS_OUT
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

float LinearizeDepth(float depth, float NearPlaneZ, float FarPlaneZ)
{
    float ProjectionA = FarPlaneZ / (FarPlaneZ - NearPlaneZ);
    float ProjectionB = (-FarPlaneZ * NearPlaneZ) / (FarPlaneZ - NearPlaneZ);

	// Sample the depth and convert to linear view space Z (assume it gets sampled as
	// a floating point value of the range [0,1])
    float linearDepth = ProjectionB / (depth - ProjectionA);

    return linearDepth;
}