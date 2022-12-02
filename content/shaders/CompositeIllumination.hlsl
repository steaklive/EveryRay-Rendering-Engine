Texture2D<float4> InputGlobalIlluminationTexture : register(t0);
Texture2D<float4> InputLocalIlluminationTexture : register(t1);

SamplerState LinearSampler : register(s0);

RWTexture2D<float4> outputTexture : register(u0);

cbuffer CompositeCB : register(b0)
{
    float4 DebugVoxelAO_Disable; // r- voxel, g - ao, b - disable composite
};

[numthreads(8, 8, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    uint width, height;
    InputLocalIlluminationTexture.GetDimensions(width, height);
    float2 texCooord = DTid.xy / float2(width, height);
    float4 localIllumination = InputLocalIlluminationTexture.SampleLevel(LinearSampler, texCooord, 0);
    if (DebugVoxelAO_Disable.b > 0.0f)
    {
        outputTexture[DTid.xy] = float4(localIllumination.rgb, 1.0f);
        return;
    }
    
    float4 globalIllumination = InputGlobalIlluminationTexture.SampleLevel(LinearSampler, texCooord, 0);    
    float ao = 1.0f - globalIllumination.a;
        
    if (DebugVoxelAO_Disable.g > 0.0f)
    {
        outputTexture[DTid.xy] = float4(ao, ao, ao, 1.0f);
        return;
    }
    
    if (DebugVoxelAO_Disable.r > 0.0f)
        outputTexture[DTid.xy] = float4(globalIllumination.rgb, 1.0f);
    else
        outputTexture[DTid.xy] = localIllumination + float4(globalIllumination.rgb * ao, 1.0f);
}