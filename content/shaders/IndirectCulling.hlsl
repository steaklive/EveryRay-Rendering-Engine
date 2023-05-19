
cbuffer MeshConstants : register(b0)
{
	uint4 IndexCount_StartIndexLoc_BaseVtxLoc_StartInstLoc;
};

cbuffer CameraConstants : register(b1)
{
	float4 FrustumPlanes[6];
	float4 LODCameraDistances;
};

struct Instance
{
	float4x4 World;
	float4 AABBmin;
	float4 AABBmax;
};
StructuredBuffer<Instance> instanceData : register(t0);
AppendStructuredBuffer<Instance> newInstanceData : register(u0);
RWBuffer<uint> argsBuffer : register(u1);

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

[numthreads(64, 1, 1)]
void CSMain(int3 DTid : SV_DispatchThreadID)
{
	int lod = 0;

	if (DTid.x == 0)
	{
		argsBuffer[lod = 0] = IndexCount_StartIndexLoc_BaseVtxLoc_StartInstLoc.x;
		argsBuffer[lod + 1] = 0;
		argsBuffer[lod + 2] = IndexCount_StartIndexLoc_BaseVtxLoc_StartInstLoc.y;
		argsBuffer[lod + 3] = IndexCount_StartIndexLoc_BaseVtxLoc_StartInstLoc.z;
		argsBuffer[lod + 4] = IndexCount_StartIndexLoc_BaseVtxLoc_StartInstLoc.w;
	}

	GroupMemoryBarrier();

	int index = DTid.x;

	Instance data = instanceData[index];

	bool isCulled = PerformFrustumCull(data.AABBmin, data.AABBmax);
	if (!isCulled)
	{
		uint instanceIndexTemp;
		InterlockedAdd(argsBuffer[lod + 1], 1, instanceIndexTemp);

		newInstanceData.Append(data);
	}
}