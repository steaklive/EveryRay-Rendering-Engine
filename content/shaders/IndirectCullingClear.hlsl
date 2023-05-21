#include "IndirectCulling.hlsli"

cbuffer MeshConstants : register(b0)
{
	uint4 IndexCount_StartIndexLoc_BaseVtxLoc_StartInstLoc[MAX_LOD_COUNT * MAX_MESH_COUNT];
	uint OriginalInstancesCount;
};

RWBuffer<uint> argsBuffer : register(u0); // MAX_LOD_COUNT * MAX_MESH_COUNT * 5

[numthreads(1, 1, 1)]
void CSMain(int3 DTid : SV_DispatchThreadID)
{
	for (int lod = 0; lod < MAX_LOD_COUNT; lod++)
	{
		for (int mesh = 0; mesh < MAX_MESH_COUNT; mesh++)
		{
			uint offset = MAX_MESH_COUNT * lod + mesh;
			argsBuffer[offset * 5 + 0] = IndexCount_StartIndexLoc_BaseVtxLoc_StartInstLoc[offset].x;
			argsBuffer[offset * 5 + 1] = 0;
			argsBuffer[offset * 5 + 2] = IndexCount_StartIndexLoc_BaseVtxLoc_StartInstLoc[offset].y;
			argsBuffer[offset * 5 + 3] = IndexCount_StartIndexLoc_BaseVtxLoc_StartInstLoc[offset].z;
			argsBuffer[offset * 5 + 4] = IndexCount_StartIndexLoc_BaseVtxLoc_StartInstLoc[offset].w;
		}
	}
}