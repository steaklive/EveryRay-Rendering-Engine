cbuffer CBufferPerObject
{
    float4x4 WorldLightViewProjection;
    float4x4 LightViewProjection; // for not breaking the legacy code...
}

struct VS_INPUT
{
    float4 ObjectPosition : POSITION;
    float2 TextureCoordinate : TEXCOORD;
};
    
struct VS_INPUT_INSTANCING
{
    float4 ObjectPosition : POSITION;
    float2 TextureCoordinate : TEXCOORD;
    row_major float4x4 World : WORLD;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float2 Depth : TexCoord0;
	float2 TextureCoordinate : TexCoord1;	
};

DepthStencilState EnableDepthDisableStencil
{
    DepthEnable = TRUE;
    DepthWriteMask = ALL;
    DepthFunc = LESS_EQUAL;
    StencilEnable = FALSE;
};

SamplerState Sampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = WRAP;
    AddressV = WRAP;
};
Texture2D AlbedoAlphaMap;

float4 create_depthmap_vertex_shader(VS_INPUT IN) : SV_Position
{
    float4 pos = mul(IN.ObjectPosition, WorldLightViewProjection);
    pos.z *= pos.w; // We want linear positions
    return pos;
}

float4 create_depthmap_vertex_shader_instancing(VS_INPUT_INSTANCING IN) : SV_Position
{
    float3 WorldPos = mul(IN.ObjectPosition, IN.World).xyz;
    float4 pos = mul(float4(WorldPos, 1.0f), LightViewProjection);
    pos.z *= pos.w; // We want linear positions
    return pos;
}


VS_OUTPUT create_depthmap_w_render_target_vertex_shader(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;

    OUT.Position = mul(IN.ObjectPosition, WorldLightViewProjection);
    OUT.Depth = OUT.Position.zw;
    OUT.TextureCoordinate = IN.TextureCoordinate;

    return OUT;
}
VS_OUTPUT create_depthmap_w_render_target_vertex_shader_instancing(VS_INPUT_INSTANCING IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;

    float3 WorldPos = mul(IN.ObjectPosition, IN.World).xyz;
    OUT.Position = mul(float4(WorldPos, 1.0f), LightViewProjection);
    OUT.Depth = OUT.Position.zw;
    OUT.TextureCoordinate = IN.TextureCoordinate;
    
    return OUT;
}

float4 create_depthmap_w_render_target_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    float alphaValue = AlbedoAlphaMap.Sample(Sampler, IN.TextureCoordinate).a;
    
    if (alphaValue > 0.1f)
        IN.Depth.x /= IN.Depth.y;
    else
        discard;

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

        SetDepthStencilState(EnableDepthDisableStencil, 0);

    }
}

//instancing
technique11 create_depthmap_instanced
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, create_depthmap_vertex_shader_instancing()));
        SetGeometryShader(NULL);
        SetPixelShader(NULL);
    }
}

technique11 create_depthmap_w_bias_instanced
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, create_depthmap_vertex_shader_instancing()));
        SetGeometryShader(NULL);
        SetPixelShader(NULL);
    }
}

technique11 create_depthmap_w_render_target_instanced
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, create_depthmap_w_render_target_vertex_shader_instancing()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, create_depthmap_w_render_target_pixel_shader()));

        SetDepthStencilState(EnableDepthDisableStencil, 0);

    }
}