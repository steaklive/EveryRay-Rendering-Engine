static const float TERRAIN_TILE_RESOLUTION = 512.0f;

RWStructuredBuffer<float4> objectsPositions : register(u0);
StructuredBuffer<float4> terrainVertices : register(t0);
Texture2D heightmap : register(t1);
Texture2D splatmap : register(t2);

cbuffer CBufferPerFrame : register(b0)
{
    float2 UVoffset;
    float HeightScale;
    float SplatChannel;
}

SamplerState BilinearSampler
{
    Filter = MIN_MAG_LINEAR_MIP_POINT;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

SamplerState LinearSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = WRAP;
    AddressV = WRAP;
    AddressW = WRAP;
};

bool GetHeightFromTriangle(float x, float z, float v0[3], float v1[3], float v2[3], float normals[3], out float height)
{
    float startVector[3], directionVector[3], edge1[3], edge2[3], normal[3];
    float Q[3], e1[3], e2[3], e3[3], edgeNormal[3], temp[3];
    float magnitude, D, denominator, numerator, t, determinant;

		// Starting position of the ray that is being cast.
    startVector[0] = x;
    startVector[1] = 0.0f;
    startVector[2] = z;

		// The direction the ray is being cast.
    directionVector[0] = 0.0f;
    directionVector[1] = -1.0f;
    directionVector[2] = 0.0f;

		// Calculate the two edges from the three points given.
    edge1[0] = v1[0] - v0[0];
    edge1[1] = v1[1] - v0[1];
    edge1[2] = v1[2] - v0[2];

    edge2[0] = v2[0] - v0[0];
    edge2[1] = v2[1] - v0[1];
    edge2[2] = v2[2] - v0[2];

		// Calculate the normal of the triangle from the two edges.
    normal[0] = (edge1[1] * edge2[2]) - (edge1[2] * edge2[1]);
    normal[1] = (edge1[2] * edge2[0]) - (edge1[0] * edge2[2]);
    normal[2] = (edge1[0] * edge2[1]) - (edge1[1] * edge2[0]);

    magnitude = (float) sqrt((normal[0] * normal[0]) + (normal[1] * normal[1]) + (normal[2] * normal[2]));
    normal[0] = normal[0] / magnitude;
    normal[1] = normal[1] / magnitude;
    normal[2] = normal[2] / magnitude;

		// Find the distance from the origin to the plane.
    D = ((-normal[0] * v0[0]) + (-normal[1] * v0[1]) + (-normal[2] * v0[2]));

		// Get the denominator of the equation.
    denominator = ((normal[0] * directionVector[0]) + (normal[1] * directionVector[1]) + (normal[2] * directionVector[2]));

		// Make sure the result doesn't get too close to zero to prevent divide by zero.
    if (abs(denominator) < 0.0001f)
    {
        return false;
    }

		// Get the numerator of the equation.
    numerator = -1.0f * (((normal[0] * startVector[0]) + (normal[1] * startVector[1]) + (normal[2] * startVector[2])) + D);

		// Calculate where we intersect the triangle.
    t = numerator / denominator;

		// Find the intersection vector.
    Q[0] = startVector[0] + (directionVector[0] * t);
    Q[1] = startVector[1] + (directionVector[1] * t);
    Q[2] = startVector[2] + (directionVector[2] * t);

		// Find the three edges of the triangle.
    e1[0] = v1[0] - v0[0];
    e1[1] = v1[1] - v0[1];
    e1[2] = v1[2] - v0[2];

    e2[0] = v2[0] - v1[0];
    e2[1] = v2[1] - v1[1];
    e2[2] = v2[2] - v1[2];

    e3[0] = v0[0] - v2[0];
    e3[1] = v0[1] - v2[1];
    e3[2] = v0[2] - v2[2];

		// Calculate the normal for the first edge.
    edgeNormal[0] = (e1[1] * normal[2]) - (e1[2] * normal[1]);
    edgeNormal[1] = (e1[2] * normal[0]) - (e1[0] * normal[2]);
    edgeNormal[2] = (e1[0] * normal[1]) - (e1[1] * normal[0]);

		// Calculate the determinant to see if it is on the inside, outside, or directly on the edge.
    temp[0] = Q[0] - v0[0];
    temp[1] = Q[1] - v0[1];
    temp[2] = Q[2] - v0[2];

    determinant = ((edgeNormal[0] * temp[0]) + (edgeNormal[1] * temp[1]) + (edgeNormal[2] * temp[2]));

		// Check if it is outside.
    if (determinant > 0.001f)
    {
        return false;
    }

		// Calculate the normal for the second edge.
    edgeNormal[0] = (e2[1] * normal[2]) - (e2[2] * normal[1]);
    edgeNormal[1] = (e2[2] * normal[0]) - (e2[0] * normal[2]);
    edgeNormal[2] = (e2[0] * normal[1]) - (e2[1] * normal[0]);

		// Calculate the determinant to see if it is on the inside, outside, or directly on the edge.
    temp[0] = Q[0] - v1[0];
    temp[1] = Q[1] - v1[1];
    temp[2] = Q[2] - v1[2];

    determinant = ((edgeNormal[0] * temp[0]) + (edgeNormal[1] * temp[1]) + (edgeNormal[2] * temp[2]));

		// Check if it is outside.
    if (determinant > 0.001f)
    {
        return false;
    }

		// Calculate the normal for the third edge.
    edgeNormal[0] = (e3[1] * normal[2]) - (e3[2] * normal[1]);
    edgeNormal[1] = (e3[2] * normal[0]) - (e3[0] * normal[2]);
    edgeNormal[2] = (e3[0] * normal[1]) - (e3[1] * normal[0]);

		// Calculate the determinant to see if it is on the inside, outside, or directly on the edge.
    temp[0] = Q[0] - v2[0];
    temp[1] = Q[1] - v2[1];
    temp[2] = Q[2] - v2[2];

    determinant = ((edgeNormal[0] * temp[0]) + (edgeNormal[1] * temp[1]) + (edgeNormal[2] * temp[2]));

		// Check if it is outside.
    if (determinant > 0.001f)
    {
        return false;
    }

		// Now we have our height.
    height = Q[1];

    return true;
}

float FindHeightFromPosition(int vertexCount, float x, float z)
{
    int index = 0;
    float vertex1[3], vertex2[3], vertex3[3], vector1[3], vector2[3], normals[3];
    float height = 0.0f;
    bool isFound = false;

    int index1, index2, index3;

    for (int i = 0; i < vertexCount / 3; i++)
    {
        index = i * 3;

        vertex1[0] = terrainVertices[index].x;
        vertex1[1] = terrainVertices[index].y;
        vertex1[2] = terrainVertices[index].z;
        index++;

        vertex2[0] = terrainVertices[index].x;
        vertex2[1] = terrainVertices[index].y;
        vertex2[2] = terrainVertices[index].z;
        index++;

        vertex3[0] = terrainVertices[index].x;
        vertex3[1] = terrainVertices[index].y;
        vertex3[2] = terrainVertices[index].z;

		// Check to see if this is the polygon we are looking for.
        isFound = GetHeightFromTriangle(x, z, vertex1, vertex2, vertex3, normals, height);
        if (isFound)
            return height;
    }
    return -1.0f;
}

float2 GetTextureCoordinates(float x, float z)
{
    float2 texcoord = (float2(x, z) + UVoffset) / TERRAIN_TILE_RESOLUTION;
    return texcoord;
}

float FindHeightFromHeightmap(float x, float z)
{
    float height = heightmap.SampleLevel(BilinearSampler, GetTextureCoordinates(x, z), 0).r;
    return height * HeightScale;
}

bool IsOnSplatMap(float x, float z, float channel)
{
    float2 uv = GetTextureCoordinates(x, z);
    uv.y = 1.0f - uv.y;
    
    float percentage = 0.2f;
    
    float4 value = splatmap.SampleLevel(LinearSampler, uv, 0);
    if (channel == 0.0f && value.r > percentage)
        return true;
    else if (channel == 1.0f && value.g > percentage)
        return true;
    else if (channel == 2.0f && value.b > percentage)
        return true;
    else if (channel == 3.0f && value.a > percentage)
        return true;
    else
        return false;
   
    return false;
}

[numthreads(512, 1, 1)]
void displaceOnTerrainCS(uint3 threadID : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex, uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID)
{
    uint vertexCount;
    uint stride;
    terrainVertices.GetDimensions(vertexCount, stride);
    
    float x = objectsPositions[threadID.x].x;
    float z = objectsPositions[threadID.x].z;
    float delta = 0.3f;
    
    if (IsOnSplatMap(x, z, SplatChannel))
        objectsPositions[threadID.x].y = FindHeightFromHeightmap(x, z) - delta; //FindHeightFromPosition(vertexCount, x, z);
    else
        objectsPositions[threadID.x].y = -999.0f;
}