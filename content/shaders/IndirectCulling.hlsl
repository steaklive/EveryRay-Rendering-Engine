#include "IndirectCulling.hlsli"

cbuffer MeshConstants : register(b0)
{
	uint4 IndexCount_StartIndexLoc_BaseVtxLoc_StartInstLoc[MAX_LOD_COUNT * MAX_MESH_COUNT];
	uint OriginalInstancesCount;
};

cbuffer CameraConstants : register(b1)
{
	float4 FrustumPlanes[6];
	float4 LODCameraSqrDistances;
	float4 CameraPos;
};

StructuredBuffer<Instance> instanceData : register(t0);
RWStructuredBuffer<Instance> newInstanceData : register(u0); // OriginalInstancesCount * MAX_LOD_COUNT
RWBuffer<uint> argsBuffer : register(u1); // MAX_LOD_COUNT * MAX_MESH_COUNT * 5

bool PerformFrustumCull(float4 aabbMin, float4 aabbMax)
{
	bool culled = false;
	float3 planeNormal = float3(0.0, 0.0, 0.0);
	float planeConstant = 0.0;
	float3 axisVert = float3(0.0, 0.0, 0.0);

	// start a loop through all frustum planes
	[loop]
	for (int planeID = 0; planeID < 6; ++planeID)
	{
		planeNormal = FrustumPlanes[planeID].xyz;
		planeConstant = FrustumPlanes[planeID].w;

		// x-axis
		if (FrustumPlanes[planeID].x > 0.0f)
			axisVert.x = aabbMin.x;
		else
			axisVert.x = aabbMax.x;

		// y-axis
		if (FrustumPlanes[planeID].y > 0.0f)
			axisVert.y = aabbMin.y;
		else
			axisVert.y = aabbMax.y;

		// z-axis
		if (FrustumPlanes[planeID].z > 0.0f)
			axisVert.z = aabbMin.z;
		else
			axisVert.z = aabbMax.z;

		if (dot(planeNormal, axisVert) + planeConstant > 0.0f)
		{
			culled = true;
			// Skip remaining planes to check and move on 
			break;
		}
	}
	return culled;
}

int CalculateLodIndex(float4x4 worldMat)
{
	float3 pos = float3(worldMat[0][3], worldMat[1][3], worldMat[2][3]);

	float distanceToCameraSqr =
		(CameraPos.x - pos.x) * (CameraPos.x - pos.x) +
		(CameraPos.y - pos.y) * (CameraPos.y - pos.y) +
		(CameraPos.z - pos.z) * (CameraPos.z - pos.z);

	if (distanceToCameraSqr <= LODCameraSqrDistances.x)
		return 0;
	else if (LODCameraSqrDistances.x < distanceToCameraSqr && distanceToCameraSqr <= LODCameraSqrDistances.y)
		return 1;
	else if (LODCameraSqrDistances.y < distanceToCameraSqr && distanceToCameraSqr <= LODCameraSqrDistances.z)
		return 2;

	return -1;
}

[numthreads(64, 1, 1)]
void CSMain(int3 DTid : SV_DispatchThreadID)
{
	int index = DTid.x;
	if (index >= OriginalInstancesCount)
		return;

	Instance data = instanceData[index];

	bool isCulled = PerformFrustumCull(data.AABBmin, data.AABBmax);
	if (!isCulled)
	{
		int lod = CalculateLodIndex(data.WorldMat);
		if (lod == -1)
			return;

		// we just need to copy the counters for all meshes in that lod
		for (int mesh = 0; mesh < MAX_MESH_COUNT; mesh++)
		{
			uint offset = MAX_MESH_COUNT * lod + mesh;

			uint outIndex;
			InterlockedAdd(argsBuffer[offset * 5 + 1], 1, outIndex);
			if (mesh == 0)
				newInstanceData[OriginalInstancesCount * lod + outIndex] = data;
		}
	}
}