RWTexture3D<float4> DstTexture : register(u0);

//TODO add constant buffer with clear value (via root constant)

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    DstTexture[DTid.xyz] = float4(0.0, 0.0, 0.0, 0.0);
}