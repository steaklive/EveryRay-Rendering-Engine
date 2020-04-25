#define vec2 float2
#define vec3 float3
#define vec4 float4

float3 ReconstructWorldPosFromDepth(float2 uv, float depth, float4x4 InvProjMatrix, float4x4 InvViewMatrix)
{
    float ndcX = uv.x * 2 - 1;
    float ndcY = 1 - uv.y * 2; // Remember to flip y!!!
    float4 viewPos = mul(float4(ndcX, ndcY, depth, 1.0f), InvProjMatrix);
    viewPos = viewPos / viewPos.w;
    return mul(viewPos, InvViewMatrix).xyz;
}