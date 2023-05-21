//These should match the cpp ones in Common.h
#define MAX_LOD_COUNT 3
#define MAX_MESH_COUNT 16

struct Instance
{
	float4x4 WorldMat;
	float4 AABBmin;
	float4 AABBmax;
};