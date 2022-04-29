#define VOLUMETRIC_FOG_VOXEL_SIZE_X 160
#define VOLUMETRIC_FOG_VOXEL_SIZE_Y 90
#define VOLUMETRIC_FOG_VOXEL_SIZE_Z 128

float LinearToExponentialDepth(float z, float NearPlaneZ, float FarPlaneZ)
{
    float z_buffer_params_y = FarPlaneZ / NearPlaneZ;
    float z_buffer_params_x = 1.0f - z_buffer_params_y;

    return (1.0f / z - z_buffer_params_y) / z_buffer_params_x;
}
float ExponentialToLinearDepth(float z, float n, float f)
{
    float z_buffer_params_y = f / n;
    float z_buffer_params_x = 1.0f - z_buffer_params_y;

    return 1.0f / (z_buffer_params_x * z + z_buffer_params_y);
}

float3 GetWorldPosFromVoxelID(uint3 texCoord, float jitter, float near, float far, float4x4 invViewProj)
{
    float viewZ = near * pow(far / near, (float(texCoord.z) + 0.5f + jitter) / float(VOLUMETRIC_FOG_VOXEL_SIZE_Z));
    float z = near / far + (float(texCoord.z) + 0.5f + jitter) / float(VOLUMETRIC_FOG_VOXEL_SIZE_Z) * (1.0f - near / far);
    float3 uv = float3((float(texCoord.x) + 0.5f) / float(VOLUMETRIC_FOG_VOXEL_SIZE_X), (float(texCoord.y) + 0.5f) / float(VOLUMETRIC_FOG_VOXEL_SIZE_Y), viewZ / far);
    
    float3 ndc;
    ndc.x = 2.0f * uv.x - 1.0f;
    ndc.y = 1.0f - 2.0f * uv.y; //turn upside down for DX
    ndc.z = 2.0f * LinearToExponentialDepth(uv.z, near, far) - 1.0f;
    
    float4 worldPos = mul(float4(ndc, 1.0f), invViewProj);
    worldPos = worldPos / worldPos.w;
    return worldPos.rgb;
}

float3 GetUVFromVolumetricFogVoxelWorldPos(float3 worldPos, float n, float f, float4x4 viewProj)
{
    float4 ndc = mul(float4(worldPos, 1.0f), viewProj);
    ndc = ndc / ndc.w;
    
    float3 uv;
    uv.x = ndc.x * 0.5f + 0.5f;
    uv.y = 0.5f - ndc.y * 0.5f; //turn upside down for DX
    uv.z = ExponentialToLinearDepth(ndc.z * 0.5f + 0.5f, n, f);
    
    float2 params = float2(float(VOLUMETRIC_FOG_VOXEL_SIZE_Z) / log2(f / n), -(float(VOLUMETRIC_FOG_VOXEL_SIZE_Z) * log2(n) / log2(f / n)));
    float view_z = uv.z * f;
    uv.z = (max(log2(view_z) * params.x + params.y, 0.0f)) / VOLUMETRIC_FOG_VOXEL_SIZE_Z;
    return uv;
}
