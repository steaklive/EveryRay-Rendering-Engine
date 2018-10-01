cbuffer CBufferPerObject
{
    float4x4 WorldLightViewProjection;
}

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float2 Depth : TEXCOORD;
};

float4 create_depthmap_vertex_shader(float4 ObjectPosition : POSITION) : SV_Position
{
    return mul(ObjectPosition, WorldLightViewProjection);
}

VS_OUTPUT create_depthmap_w_render_target_vertex_shader(float4 ObjectPosition : POSITION)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;

    OUT.Position = mul(ObjectPosition, WorldLightViewProjection);
    OUT.Depth = OUT.Position.zw;

    return OUT;
}

float4 create_depthmap_w_render_target_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    IN.Depth.x /= IN.Depth.y;

    return float4(IN.Depth.x, 0, 0, 1);
}

technique11 create_depthmap
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, create_depthmap_vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(NULL);
    }
}

technique11 create_depthmap_w_bias
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, create_depthmap_vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(NULL);
    }
}

technique11 create_depthmap_w_render_target
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, create_depthmap_w_render_target_vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, create_depthmap_w_render_target_pixel_shader()));
    }
}