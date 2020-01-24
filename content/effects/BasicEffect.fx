cbuffer CBufferPerObject
{
	float4x4 WorldViewProjection : WORLDVIEWPROJECTION; 
}
struct VS_INPUT
{
	float4 ObjectPosition: POSITION;
	float4 Color : COLOR;
};
struct VS_OUTPUT 
{
	float4 Position: SV_Position;
	float4 Color : COLOR;
};
RasterizerState DisableCulling
{
    CullMode = NONE;
};
VS_OUTPUT vertex_shader(VS_INPUT IN)
{
	VS_OUTPUT OUT = (VS_OUTPUT)0;
	
    OUT.Position = mul(IN.ObjectPosition, WorldViewProjection);
	OUT.Color = IN.Color;
	
	return OUT;
}
float4 pixel_shader(VS_OUTPUT IN) : SV_Target
{
    return IN.Color;
}
technique11 main11
{
    pass p0
	{
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
		SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, pixel_shader()));

		SetRasterizerState(DisableCulling);
    }
}