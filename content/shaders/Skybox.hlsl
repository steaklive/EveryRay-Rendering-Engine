cbuffer SkyboCBuffer : register(b0)
{
    float4x4 WorldViewProjection;
    float4 SunColor;
    float4 TopColor;
    float4 BottomColor;
    float2 SkyHeight;
}

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

VS_OUTPUT VSMain(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT)0;
    
    OUT.Position = mul(IN.ObjectPosition, WorldViewProjection);
    OUT.TextureCoordinate = IN.ObjectPosition.xyz;
    OUT.SkyboxPos = IN.ObjectPosition;
    return OUT;
}

float4 PSMain(VS_OUTPUT IN) : SV_Target
{
    float4 color;
    float height = IN.SkyboxPos.y;
    
    color = lerp(BottomColor, TopColor, smoothstep(SkyHeight.x, SkyHeight.y, height));
    return saturate(color * SunColor);
}