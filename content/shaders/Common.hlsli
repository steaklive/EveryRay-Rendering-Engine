static const float PI = 3.141592654f;
static const float EPSILON = 0.0001f;

// Bitmasks for "RenderingObjectFlags" as decimal values
// Keep in sync with ER_RenderingObject.h!
static const uint RENDERING_OBJECT_FLAG_USE_GLOBAL_DIF_PROBE    = 1;    //  0000000000001 // use global diffuse probe
static const uint RENDERING_OBJECT_FLAG_USE_GLOBAL_SPEC_PROBE   = 2;    //  0000000000010 // use global specular probe
static const uint RENDERING_OBJECT_FLAG_SSS                     = 4;    //  0000000000100 // use sss
static const uint RENDERING_OBJECT_FLAG_POM                     = 8;    //  0000000001000 // use pom
static const uint RENDERING_OBJECT_FLAG_REFLECTION              = 16;   //  0000000010000 // use reflection mask
static const uint RENDERING_OBJECT_FLAG_FOLIAGE                 = 32;   //  0000000100000 // is foliage
static const uint RENDERING_OBJECT_FLAG_TRANSPARENT             = 64;   //  0000001000000 // is transparent
static const uint RENDERING_OBJECT_FLAG_FUR                     = 128;  //  0000010000000 // is fur
static const uint RENDERING_OBJECT_FLAG_SKIP_DEFERRED_PASS      = 256;  //  0000100000000 // skip deferred pass
static const uint RENDERING_OBJECT_FLAG_SKIP_INDIRECT_DIF       = 512;  //  0001000000000 // skip indirect specular lighting
static const uint RENDERING_OBJECT_FLAG_SKIP_INDIRECT_SPEC      = 1024; //  0010000000000 // skip indirect specular lighting
static const uint RENDERING_OBJECT_FLAG_GPU_INDIRECT_DRAW       = 2048; //  0100000000000 // used for GPU indirect drawing
static const uint RENDERING_OBJECT_FLAG_TRIPLANAR_MAPPING       = 4096; //  1000000000000 // use triplanar mapping

cbuffer ObjectCBuffer : register(b1)
{
    float4x4 World;
    float IndexOfRefraction;
    float CustomRoughness;
    float CustomMetalness;
    float CustomAlphaDiscard;
    uint OriginalInstanceCount;
    uint RenderingObjectFlags;
};

struct QUAD_VS_IN
{
    float4 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct QUAD_VS_OUT
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

float LinearizeDepth(float depth, float NearPlaneZ, float FarPlaneZ)
{
    float ProjectionA = FarPlaneZ / (FarPlaneZ - NearPlaneZ);
    float ProjectionB = (-FarPlaneZ * NearPlaneZ) / (FarPlaneZ - NearPlaneZ);

	// Sample the depth and convert to linear view space Z (assume it gets sampled as
	// a floating point value of the range [0,1])
    float linearDepth = ProjectionB / (depth - ProjectionA);

    return linearDepth;
}

float3 ReconstructWorldPosFromDepth(float2 uv, float depth, float4x4 invProj, float4x4 invView)
{
    float ndcX = uv.x * 2 - 1;
    float ndcY = 1 - uv.y * 2; // Remember to flip y!!!
    float4 viewPos = mul(invProj, float4(ndcX, ndcY, depth, 1.0f));
    viewPos = viewPos / viewPos.w;
    return mul(invView, viewPos).xyz;
}

float3 ReconstructViewPosFromDepth(float2 uv, float depth, float4x4 invProj)
{
    float ndcX = uv.x * 2 - 1;
    float ndcY = 1 - uv.y * 2; // Remember to flip y!!!
    float4 viewPos = mul(invProj, float4(ndcX, ndcY, depth, 1.0f));
    viewPos = viewPos / viewPos.w;
    return viewPos.xyz;
}

float3 GetTriplanarMappingWeights(float3 aWorldNormal, float aSharpness)
{
    float3 blendWeights = pow(abs(aWorldNormal), aSharpness);
    blendWeights /= dot(blendWeights, float3(1, 1, 1));

    return blendWeights;
}

#define TRIPLANAR_MAPPING_TEXTURE_SCALE  8.0 //TODO: should be parsed properly

// triplanar mapping result 4 channel input
float4 GetTriplanarMapping(Texture2D<float4> aTexture, SamplerState aSampler, float3 aWorldPos, float3 aWorldNormal, float aSharpness)
{   
    float4 xTex = aTexture.Sample(aSampler, aWorldPos.zy / TRIPLANAR_MAPPING_TEXTURE_SCALE);
    float4 yTex = aTexture.Sample(aSampler, aWorldPos.xz / TRIPLANAR_MAPPING_TEXTURE_SCALE);
    float4 zTex = aTexture.Sample(aSampler, aWorldPos.xy / TRIPLANAR_MAPPING_TEXTURE_SCALE);
    
    float3 blendWeights = GetTriplanarMappingWeights(aWorldNormal, aSharpness);
    return blendWeights.x * xTex + blendWeights.y * yTex + blendWeights.z * zTex;
}

// triplanar mapping result 1 channel input
float GetTriplanarMapping(Texture2D<float> aTexture, SamplerState aSampler, float3 aWorldPos, float3 aWorldNormal, float aSharpness)
{
    float xTex = aTexture.Sample(aSampler, aWorldPos.zy / TRIPLANAR_MAPPING_TEXTURE_SCALE);
    float yTex = aTexture.Sample(aSampler, aWorldPos.xz / TRIPLANAR_MAPPING_TEXTURE_SCALE);
    float zTex = aTexture.Sample(aSampler, aWorldPos.xy / TRIPLANAR_MAPPING_TEXTURE_SCALE);

    float3 blendWeights = GetTriplanarMappingWeights(aWorldNormal, aSharpness);
    return blendWeights.x * xTex + blendWeights.y * yTex + blendWeights.z * zTex;
}