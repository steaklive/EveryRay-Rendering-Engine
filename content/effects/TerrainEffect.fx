cbuffer CBufferPerObject
{
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
}
struct VS_INPUT
{
	float4 ObjectPosition: POSITION;
	float3 Normal : NORMAL;
};
struct VS_OUTPUT 
{
	float4 Position: SV_Position;
    float3 Normal : NORMAL;
};

VS_OUTPUT vertex_shader(VS_INPUT IN)
{
	VS_OUTPUT OUT = (VS_OUTPUT)0;
	
    OUT.Position = mul(IN.ObjectPosition, World);
    OUT.Position = mul(OUT.Position, View);
    OUT.Position = mul(OUT.Position, Projection);
    OUT.Normal = normalize(mul(float4(IN.Normal, 0), World).xyz);
	
	return OUT;
}
float4 pixel_shader(VS_OUTPUT IN) : SV_Target
{
    float3 lightDir;
    float lightIntensity;
    float4 color;
    
    color = float4(0.5f, 0.5f, 0.5f, 1.0f);

    // Invert the light direction for calculations.
    lightDir = -float3(0.2f, -0.7f, 0.0f);

    // Calculate the amount of light on this pixel.
    lightIntensity = saturate(dot(IN.Normal, lightDir));

    if (lightIntensity > 0.0f)
    {
        // Determine the final diffuse color based on the diffuse color and the amount of light intensity.
        color += (float4(0.5f, 0.5f, 0.5f, 1.0f) * lightIntensity);
    }

    // Saturate the final light color.
    color = saturate(color);

    return color;
}
technique11 main
{
    pass p0
	{
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
		SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, pixel_shader()));
    }
}