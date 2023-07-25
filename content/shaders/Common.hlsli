#define NUM_OF_SHADOW_CASCADES 3

static const float PI = 3.141592654f;
static const float EPSILON = 0.0001f;

// Bitmasks for "RenderingObjectFlags" as decimal values
// Keep in sync with ER_RenderingObject.h!
static const uint RENDERING_OBJECT_FLAG_USE_GLOBAL_DIF_PROBE    = 1;    //  000000000001 // use global diffuse probe
static const uint RENDERING_OBJECT_FLAG_USE_GLOBAL_SPEC_PROBE   = 2;    //  000000000010 // use global specular probe
static const uint RENDERING_OBJECT_FLAG_SSS                     = 4;    //  000000000100 // use sss
static const uint RENDERING_OBJECT_FLAG_POM                     = 8;    //  000000001000 // use pom
static const uint RENDERING_OBJECT_FLAG_REFLECTION              = 16;   //  000000010000 // use reflection mask
static const uint RENDERING_OBJECT_FLAG_FOLIAGE                 = 32;   //  000000100000 // is foliage
static const uint RENDERING_OBJECT_FLAG_TRANSPARENT             = 64;   //  000001000000 // is transparent
static const uint RENDERING_OBJECT_FLAG_FUR                     = 128;  //  000010000000 // is fur
static const uint RENDERING_OBJECT_FLAG_SKIP_DEFERRED_PASS      = 256;  //  000100000000 // skip deferred pass
static const uint RENDERING_OBJECT_FLAG_SKIP_INDIRECT_DIF       = 512;  //  001000000000 // skip indirect specular lighting
static const uint RENDERING_OBJECT_FLAG_SKIP_INDIRECT_SPEC      = 1024; //  010000000000 // skip indirect specular lighting
static const uint RENDERING_OBJECT_FLAG_GPU_INDIRECT_DRAW       = 2048; //  100000000000 // used for GPU indirect drawing

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