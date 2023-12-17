Texture2D<float4> InputGlobalIlluminationTexture : register(t0);
Texture2D<float4> InputLocalIlluminationTexture : register(t1);

SamplerState LinearSampler : register(s0);

RWTexture2D<float4> outputTexture : register(u0);

cbuffer CompositeCB : register(b0)
{
    uint CompositeFlags;
};

// Keep in sync with ER_Illumination!
#define	COMPOSITE_FLAGS_NONE                          0x00000000
#define COMPOSITE_FLAGS_NO_GI                         0x00000001
#define COMPOSITE_FLAGS_DEBUG_GI_AO                   0x00000002
#define COMPOSITE_FLAGS_DEBUG_GI_IRRADIANCE           0x00000003
#define COMPOSITE_FLAGS_DEBUG_GBUFFER_ALBEDO          0x00000004
#define COMPOSITE_FLAGS_DEBUG_GBUFFER_NORMALS         0x00000005
#define COMPOSITE_FLAGS_DEBUG_GBUFFER_ROUGHNESS       0x00000006
#define COMPOSITE_FLAGS_DEBUG_GBUFFER_METALNESS       0x00000007

[numthreads(8, 8, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    uint width, height;
    InputLocalIlluminationTexture.GetDimensions(width, height);
    float2 texCooord = DTid.xy / float2(width, height);
    float4 localIllumination = InputLocalIlluminationTexture.SampleLevel(LinearSampler, texCooord, 0);
    
    if (CompositeFlags > COMPOSITE_FLAGS_DEBUG_GI_IRRADIANCE)
    {
        if (CompositeFlags == COMPOSITE_FLAGS_DEBUG_GBUFFER_ALBEDO)
            outputTexture[DTid.xy] = float4(pow(localIllumination.rgb, 2.2), 1.0f);
        else if (CompositeFlags == COMPOSITE_FLAGS_DEBUG_GBUFFER_NORMALS)
            outputTexture[DTid.xy] = float4(normalize(localIllumination.rgb), 1.0f);
        else if (CompositeFlags == COMPOSITE_FLAGS_DEBUG_GBUFFER_ROUGHNESS)
            outputTexture[DTid.xy] = float4(localIllumination.ggg, 1.0f);
        else if (CompositeFlags == COMPOSITE_FLAGS_DEBUG_GBUFFER_METALNESS)
            outputTexture[DTid.xy] = float4(localIllumination.bbb, 1.0f);
        return;
    }
    
    float4 globalIllumination = InputGlobalIlluminationTexture.SampleLevel(LinearSampler, texCooord, 0);    
    float ao = 1.0f - globalIllumination.a;
        
    if (CompositeFlags == COMPOSITE_FLAGS_NO_GI)
        outputTexture[DTid.xy] = float4(localIllumination.rgb, 1.0f);
    else if (CompositeFlags == COMPOSITE_FLAGS_DEBUG_GI_AO)
        outputTexture[DTid.xy] = float4(ao, ao, ao, 1.0f);
    else if (CompositeFlags == COMPOSITE_FLAGS_DEBUG_GI_IRRADIANCE)
         outputTexture[DTid.xy] = float4(globalIllumination.rgb, 1.0f);
    else
        outputTexture[DTid.xy] = localIllumination + float4(globalIllumination.rgb * ao, 1.0f);
}