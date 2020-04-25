#define vec2 float2
#define vec3 float3
#define vec4 float4

Texture2D ColorTexture;
Texture2D GBufferDepth;
Texture2D GBufferNormals;
Texture2D GBufferExtra;
//Texture2D GBufferPositions;

SamplerState Sampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = WRAP;
    AddressV = WRAP;
};

cbuffer CBufferPerObject
{
    float4x4 ViewProjection;
    float4x4 InvProjMatrix;
    float4x4 InvViewMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
	float4 CameraPosition;
    float StepSize;
    float MaxThickness;
    float Time;
    int MaxRayCount;
}

struct VS_INPUT
{
    float4 Position : POSITION;
    float2 TextureCoordinate : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

/************* Vertex Shader *************/

VS_OUTPUT vertex_shader(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;
    
    OUT.Position = IN.Position; //mul(IN.Position, WorldViewProjection);
    OUT.TexCoord = IN.TextureCoordinate;
    
    return OUT;
}
float linearizeDepth(float depth, float NearPlaneZ, float FarPlaneZ)
{
    float ProjectionA = FarPlaneZ / (FarPlaneZ - NearPlaneZ);
    float ProjectionB = (-FarPlaneZ * NearPlaneZ) / (FarPlaneZ - NearPlaneZ);

	// Sample the depth and convert to linear view space Z (assume it gets sampled as
	// a floating point value of the range [0,1])
    float linearDepth = ProjectionB / (depth - ProjectionA);

    return linearDepth;
}

float noise(float2 seed)
{
    return frac(sin(dot(seed.xy, float2(12.9898, 78.233))) * 43758.5453);
}
vec4 Raytrace(vec3 reflectionWorld, const int maxCount, float stepSize, float3 pos, float2 uv)
{
    vec4 color = vec4(0.0, 0.0f, 0.0f, 0.0f);		
    float3 step = StepSize * reflectionWorld;
    float4x4 projView = mul(ViewMatrix, ProjMatrix);
    bool success = false;
    
    for (int i = 1; i <= maxCount; i++)
    {
        float3 ray = (i + noise(uv + Time)) * step;
        float3 rayPos = pos + ray;
        float4 vpPos = mul(float4(rayPos, 1.0f), projView);
        float2 rayUv = vpPos.xy / vpPos.w * float2(0.5f, -0.5f) + float2(0.5f, -0.5f);

        float rayDepth = vpPos.z / vpPos.w;
        float gbufferDepth = GBufferDepth.Sample(Sampler, rayUv).r;
        if (rayDepth - gbufferDepth > 0 && rayDepth - gbufferDepth < MaxThickness)
        {            
            float a = 0.3f * pow(min(1.0, (StepSize * maxCount / 2) / length(ray)), 2.0);
            color = color * (1.0f - a) + float4(ColorTexture.Sample(Sampler, rayUv).rgb, 1.0f) * a;
            //success = true;
            break;
        }
    
    }
	
    //if (!success)
    //{
    //    i.e read from local cubemap
    //}
	
    return color;
}
float3 ReconstructWorldPosFromDepth(float2 uv, float depth)
{
    float ndcX = uv.x * 2 - 1;
    float ndcY = 1 - uv.y * 2; // Remember to flip y!!!
    float4 viewPos = mul(float4(ndcX, ndcY, depth, 1.0f), InvProjMatrix);
    viewPos = viewPos / viewPos.w;
    return mul(viewPos, InvViewMatrix).xyz;
}

/************* Pixel Shaders *************/

float4 pixel_shader(VS_OUTPUT IN) : SV_Target
{
    float4 color = ColorTexture.Sample(Sampler, IN.TexCoord);
    float reflectionMaskFactor = GBufferExtra.Sample(Sampler, IN.TexCoord).r;
    if (reflectionMaskFactor == 0.0f)
        return color;
    
	float depth = GBufferDepth.Sample(Sampler, IN.TexCoord).r;
    if (depth >= 1.0f) 
        return color;
    
    float4 normal = GBufferNormals.Sample(Sampler, IN.TexCoord);// * 2.0 - 1.0;
    float4 worldSpacePosition = float4(ReconstructWorldPosFromDepth(IN.TexCoord, depth), 1.0f);
    float4 camDir = normalize(worldSpacePosition - CameraPosition);
    float3 refDir = normalize(reflect(normalize(camDir), normal));

    int maxCountSize = 50;
	vec4 reflectedColor = Raytrace(refDir, maxCountSize, StepSize, worldSpacePosition.rgb, IN.TexCoord);
    
    return color + reflectedColor;
}

float4 pixel_shader_no_ssr(VS_OUTPUT IN) : SV_Target
{
    return ColorTexture.Sample(Sampler, IN.TexCoord);
}

technique11 ssr
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, pixel_shader()));
    }
}
technique11 no_ssr
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, pixel_shader_no_ssr()));
    }
}
