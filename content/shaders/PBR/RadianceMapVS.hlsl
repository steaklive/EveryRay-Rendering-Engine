cbuffer VSConstants
{
	float2  HalfPixel;
	float	Face;
	float	MipIndex;
};

struct VSInput
{
	float4 Position 	: POSITION;
	float2 TexCoord 	: TEXCOORD0;
};

struct VSOutput
{
	float4 Position			: SV_Position;
	float2 TexCoord			: TEXCOORD0;
};

//=================================================================================================
// Vertex Shader
//=================================================================================================
VSOutput main(VSInput input)
{
	VSOutput output;

	output.Position = float4(input.Position.xyz, 1.0f);
	output.TexCoord = input.TexCoord;

	return output;
}