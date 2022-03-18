// ================================================================================================
// Vertex/Pixel shader for simple color rendering. 
//
// TODO: add instancing support
//
// Written by Gen Afanasev for 'EveryRay Rendering Engine', 2017-2022
// ================================================================================================

cbuffer BasicColorCBuffer : register (b0)
{
    float4x4 World;
    float4x4 ViewProjection;
    float4 Color;
}
struct VS_INPUT
{
    float4 Position : POSITION;
};
struct VS_OUTPUT
{
    float4 Position : SV_Position;
};

VS_OUTPUT VSMain(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;
	
    OUT.Position = mul(IN.Position, mul(World, ViewProjection));
    return OUT;
}
float4 PSMain(VS_OUTPUT IN) : SV_Target
{
    return Color;
}