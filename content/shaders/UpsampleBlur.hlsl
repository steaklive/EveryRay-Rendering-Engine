// Generic upsampling and blurring CS shader
//
// Modified version of https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/UpsampleAndBlurCS.hlsl

cbuffer UpsampleAndBlurCbuffer : register(b0)
{
    bool Upsample = true;
};

Texture2D<float4> Input : register(t0);
RWTexture2D<float4> Output : register(u0);

SamplerState BilinearSampler : register(s0);

// The guassian blur weights (derived from Pascal's triangle)
static const float Weights5[3] = { 6.0f / 16.0f, 4.0f / 16.0f, 1.0f / 16.0f };
static const float Weights7[4] = { 20.0f / 64.0f, 15.0f / 64.0f, 6.0f / 64.0f, 1.0f / 64.0f };
static const float Weights9[5] = { 70.0f / 256.0f, 56.0f / 256.0f, 28.0f / 256.0f, 8.0f / 256.0f, 1.0f / 256.0f };

float4 Blur5(float4 a, float4 b, float4 c, float4 d, float4 e, float4 f, float4 g, float4 h, float4 i)
{
    return Weights5[0] * e + Weights5[1] * (d + f) + Weights5[2] * (c + g);
}
float4 Blur7(float4 a, float4 b, float4 c, float4 d, float4 e, float4 f, float4 g, float4 h, float4 i)
{
    return Weights7[0] * e + Weights7[1] * (d + f) + Weights7[2] * (c + g) + Weights7[3] * (b + h);
}
float4 Blur9(float4 a, float4 b, float4 c, float4 d, float4 e, float4 f, float4 g, float4 h, float4 i)
{
    return Weights9[0] * e + Weights9[1] * (d + f) + Weights9[2] * (c + g) + Weights9[3] * (b + h) + Weights9[4] * (a + i);
}
#define BlurPixels Blur9

// 16x16 pixels with an 8x8 center that we will be blurring writing out.  Each uint is two color channels packed together
groupshared uint CacheR[128];
groupshared uint CacheG[128];
groupshared uint CacheB[128];
groupshared uint CacheA[128];

void Store2Pixels(uint index, float4 pixel1, float4 pixel2)
{
    CacheR[index] = f32tof16(pixel1.r) | f32tof16(pixel2.r) << 16;
    CacheG[index] = f32tof16(pixel1.g) | f32tof16(pixel2.g) << 16;
    CacheB[index] = f32tof16(pixel1.b) | f32tof16(pixel2.b) << 16;
    CacheA[index] = f32tof16(pixel1.a) | f32tof16(pixel2.a) << 16;
}

void Load2Pixels(uint index, out float4 pixel1, out float4 pixel2)
{
    uint rr = CacheR[index];
    uint gg = CacheG[index];
    uint bb = CacheB[index];
    uint aa = CacheA[index];
    pixel1 = float4(f16tof32(rr), f16tof32(gg), f16tof32(bb), f16tof32(aa));
    pixel2 = float4(f16tof32(rr >> 16), f16tof32(gg >> 16), f16tof32(bb >> 16), f16tof32(aa >> 16));
}

void Store1Pixel(uint index, float4 pixel)
{
    CacheR[index] = asuint(pixel.r);
    CacheG[index] = asuint(pixel.g);
    CacheB[index] = asuint(pixel.b);
    CacheA[index] = asuint(pixel.a);
}

void Load1Pixel(uint index, out float4 pixel)
{
    pixel = asfloat(uint4(CacheR[index], CacheG[index], CacheB[index], CacheA[index]));
}

// Blur two pixels horizontally.  This reduces LDS reads and pixel unpacking.
void BlurHorizontally(uint outIndex, uint leftMostIndex)
{
    float4 s0, s1, s2, s3, s4, s5, s6, s7, s8, s9;
    Load2Pixels(leftMostIndex + 0, s0, s1);
    Load2Pixels(leftMostIndex + 1, s2, s3);
    Load2Pixels(leftMostIndex + 2, s4, s5);
    Load2Pixels(leftMostIndex + 3, s6, s7);
    Load2Pixels(leftMostIndex + 4, s8, s9);
    
    Store1Pixel(outIndex, BlurPixels(s0, s1, s2, s3, s4, s5, s6, s7, s8));
    Store1Pixel(outIndex + 1, BlurPixels(s1, s2, s3, s4, s5, s6, s7, s8, s9));
}

void BlurVertically(uint2 pixelCoord, uint topMostIndex)
{
    float4 s0, s1, s2, s3, s4, s5, s6, s7, s8;
    Load1Pixel(topMostIndex, s0);
    Load1Pixel(topMostIndex + 8, s1);
    Load1Pixel(topMostIndex + 16, s2);
    Load1Pixel(topMostIndex + 24, s3);
    Load1Pixel(topMostIndex + 32, s4);
    Load1Pixel(topMostIndex + 40, s5);
    Load1Pixel(topMostIndex + 48, s6);
    Load1Pixel(topMostIndex + 56, s7);
    Load1Pixel(topMostIndex + 64, s8);

    Output[pixelCoord] = BlurPixels(s0, s1, s2, s3, s4, s5, s6, s7, s8);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    uint inputWidth = 0;
    uint inputHeight = 0;
    Output.GetDimensions(inputWidth, inputHeight);

    // Load 4 pixels per thread into LDS
    int2 GroupUL = (Gid.xy << 3) - 4; // Upper-left pixel coordinate of group read location
    int2 ThreadUL = (GTid.xy << 1) + GroupUL; // Upper-left pixel coordinate of quad that this thread will read
    // Store 4 unblurred pixels in LDS
    int destIdx = GTid.x + (GTid.y << 4);
    
    if (Upsample)
    {
        float2 uvUL = (float2(ThreadUL) + 0.5) * float2(1.0f / inputWidth, 1.0f / inputHeight);
        float2 uvLR = uvUL + float2(1.0f / inputWidth, 1.0f / inputHeight);
        float2 uvUR = float2(uvLR.x, uvUL.y);
        float2 uvLL = float2(uvUL.x, uvLR.y);
    
        Store2Pixels(destIdx + 0, Input.SampleLevel(BilinearSampler, uvUL, 0.0f), Input.SampleLevel(BilinearSampler, uvUR, 0.0f));
        Store2Pixels(destIdx + 8, Input.SampleLevel(BilinearSampler, uvLL, 0.0f), Input.SampleLevel(BilinearSampler, uvLR, 0.0f));
    }
    else
    {
        Store2Pixels(destIdx + 0, Input[ThreadUL + uint2(0, 0)], Input[ThreadUL + uint2(1, 0)]);
        Store2Pixels(destIdx + 8, Input[ThreadUL + uint2(0, 1)], Input[ThreadUL + uint2(1, 1)]);
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    uint row = GTid.y << 4;
    BlurHorizontally(row + (GTid.x << 1), row + GTid.x + (GTid.x & 4));
    GroupMemoryBarrierWithGroupSync();
    BlurVertically(DTid.xy, (GTid.y << 3) + GTid.x);
}
