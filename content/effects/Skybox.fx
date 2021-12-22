/************* Resources *************/

cbuffer CBufferPerObject
{
    float4x4 WorldViewProjection : WORLDVIEWPROJECTION < string UIWidget="None"; >;
    
    float4 SunColor;
    float4 TopColor;
    float4 BottomColor;
    float UseCustomColor;
}

TextureCube SkyboxTexture <
    string UIName =  "Skybox Texture";
    string ResourceType = "3D";
>;

SamplerState TrilinearSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
};

SamplerState SamplerAnisotropic
{
    Filter = ANISOTROPIC;
    MaxAnisotropy = 16;
    AddressU = Wrap;
    AddressV = Wrap;
};

SamplerState NoFiltering;

RasterizerState DisableCulling
{
    CullMode = NONE;
};

/************* Data Structures *************/

struct VS_INPUT
{
    float4 ObjectPosition : POSITION;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float3 TextureCoordinate : TEXCOORD;
    float4 SkyboxPos : TEXCOORD1;
};

/************* Vertex Shader *************/

VS_OUTPUT vertex_shader(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT)0;
    
    OUT.Position = mul(IN.ObjectPosition, WorldViewProjection);
    OUT.TextureCoordinate = IN.ObjectPosition.xyz;
    OUT.SkyboxPos = IN.ObjectPosition;
    return OUT;
}

/************* Pixel Shader *************/

float4 pixel_shader(VS_OUTPUT IN) : SV_Target
{
    if (UseCustomColor == 1.0f)
    {
        float4 color;
        float height = IN.SkyboxPos.y;
        if (height < 0.0f)
            height = 0.0f;

        color = lerp(BottomColor, TopColor, height);
        return saturate(color * SunColor);
    }
    else 
        return saturate(SkyboxTexture.Sample(NoFiltering, IN.TextureCoordinate) * SunColor);
}

/************* Techniques *************/

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