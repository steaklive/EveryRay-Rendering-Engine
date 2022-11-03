Texture2D<float4> SrcTexture : register(t0);
RWTexture2D<float4> DstTexture : register(u0);

SamplerState BilinearClamp : register(s0);

cbuffer GenerateMips2DCB : register(b0)
{
    uint3 Size_IsSRGB;
}

float3 ApplySRGBCurve(float3 x)
{
    // This is cheaper but nearly equivalent
    return x < 0.0031308 ? 12.92 * x : 1.13005 * sqrt(abs(x - 0.00228)) - 0.13448 * x + 0.005719;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    float2 texelSize = 1.0f / float2(Size_IsSRGB.xy);
    
    //float2 texcoords = texelSize * (DTid.xy + 0.5);
    //float4 color = SrcTexture.SampleLevel(BilinearClamp, texcoords, 0);
    
    // Use 4 bilinear samples to guarantee we don't undersample when downsizing by more than 2x
    float2 texcoords = texelSize * (DTid.xy + float2(0.25, 0.25));
    float2 O = texelSize * 0.5;
    float4 color = SrcTexture.SampleLevel(BilinearClamp, texcoords, 0);
    color += SrcTexture.SampleLevel(BilinearClamp, texcoords + float2(O.x, 0.0), 0);
    color += SrcTexture.SampleLevel(BilinearClamp, texcoords + float2(0.0, O.y), 0);
    color += SrcTexture.SampleLevel(BilinearClamp, texcoords + float2(O.x, O.y), 0);
    color *= 0.25;
    
    if (Size_IsSRGB.z > 0)
        DstTexture[DTid.xy] = float4(ApplySRGBCurve(color.rgb), color.a);
    else
        DstTexture[DTid.xy] = color;
}