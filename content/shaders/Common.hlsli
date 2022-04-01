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