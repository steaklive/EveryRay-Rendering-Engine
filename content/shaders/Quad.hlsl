struct VS_IN
{
    float4 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct VS_OUT
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

VS_OUT VSMain(VS_IN input)
{
    VS_OUT output;

    output.Position = float4(input.Position.xyz, 1.0f);
    output.TexCoord = input.TexCoord;

    return output;
}
