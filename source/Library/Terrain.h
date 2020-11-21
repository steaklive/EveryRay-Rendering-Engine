#pragma once
#include "Common.h"
#include "GameComponent.h"
#include "Camera.h"
#include "DirectionalLight.h"
#include "PostProcessingStack.h"

static const int NUM_THREADS_PER_TERRAIN_SIDE = 4;
static const int NUM_PATCHES = 8;
static const int TERRAIN_TILE_RESOLUTION = 512;

namespace Library 
{
	class TerrainMaterial;

	enum TerrainSplatChannels {
		MUD,
		GRASS,
		ROCK,
		GROUND
	};

	struct NormalVector
	{
		float x, y, z;
	};

	class HeightMap
	{
		struct MapData
		{
			float x, y, z;
			float normalX, normalY, normalZ;
			float u, v;
			float uTile, vTile;
		};

		struct Vertex {
			float x, y, z;
		};

	public:
		bool GetHeightFromTriangle(float x, float z, float v0[3], float v1[3], float v2[3], float normal[3], float& height);
		bool RayIntersectsTriangle(float x, float z, float v0[3], float v1[3], float v2[3], float normals[3], float& height);
		float FindHeightFromPosition(float x, float z);

		HeightMap(int width, int height);
		~HeightMap();

		ID3D11Buffer* mVertexBuffer = nullptr;
		ID3D11Buffer* mVertexBufferTS = nullptr;
		ID3D11Buffer* mIndexBuffer = nullptr;
		ID3D11ShaderResourceView* mSplatTexture = nullptr;
		ID3D11ShaderResourceView* mHeightTexture = nullptr;
		ID3D11ShaderResourceView* mNormalTexture = nullptr;
		int mVertexCount = 0;
		int mIndexCount = 0;
		XMMATRIX mWorldMatrix = XMMatrixIdentity();
		XMMATRIX mWorldMatrixTS = XMMatrixIdentity();

		XMFLOAT2 mUVOffsetToTextureSpace;

		Vertex mVertexList[(TERRAIN_TILE_RESOLUTION-1) * (TERRAIN_TILE_RESOLUTION - 1)*6];

		MapData* mData = nullptr;
	};

	class Terrain : public GameComponent
	{
	public:
		Terrain(std::string path, Game& game, Camera& camera, DirectionalLight& light, Rendering::PostProcessingStack& pp, bool isWireframe);
		~Terrain();

		UINT GetWidth() { return mWidth; }
		UINT GetHeight() { return mHeight; }

		void Draw();
		void Draw(int tileIndex);
		void Update();

		void SetWireframeMode(bool flag) { mIsWireframe = flag; }
		void SetTessellationTerrainMode(bool flag) { mUseTessellatedTerrain = flag; }
		void SetNormalTerrainMode(bool flag) { mUseNonTessellatedTerrain = flag; }
		void SetDynamicTessellation(bool flag) { mUseDynamicTessellation = flag; }
		void SetTessellationFactor(int tessellationFactor) { mTessellationFactor = tessellationFactor; }
		void SetDynamicTessellationDistanceFactor(float factor) { mDistanceFactor = factor; }
		void SetTessellationFactorDynamic(float factor) { mTessellationFactorDynamic = factor; }
		void SetTerrainHeightScale(float scale) { mTerrainHeightScale = scale; }
		HeightMap* GetHeightmap(int index) { return mHeightMaps.at(index); }

	private:
		Camera& mCamera;

		void LoadTextures(std::string path);
		void LoadTileGroup(int threadIndex, std::string path);
		void GenerateTileMesh(int tileIndex);
		void LoadRawHeightmapPerTileCPU(int tileIndexX, int tileIndexY, std::string path);
		void LoadSplatmapPerTileGPU(int tileIndexX, int tileIndexY, std::string path);
		void LoadHeightmapPerTileGPU(int tileIndexX, int tileIndexY, std::string path);
		void LoadNormalmapPerTileGPU(int tileIndexX, int tileIndexY, std::string path);
		void DrawTessellated(int i);
		void DrawNonTessellated(int i);

		TerrainMaterial* mMaterial;

		DirectionalLight& mDirectionalLight;

		Rendering::PostProcessingStack& mPPStack;

		UINT mWidth = 0;
		UINT mHeight = 0;
		
		std::vector<HeightMap*> mHeightMaps;

		ID3D11ShaderResourceView* mGrassTexture = nullptr;
		ID3D11ShaderResourceView* mGroundTexture = nullptr;
		ID3D11ShaderResourceView* mRockTexture = nullptr;
		ID3D11ShaderResourceView* mMudTexture = nullptr;

		ID3D11Buffer* mVertexPatchBuffer = nullptr;

		float mHeightScale = 200.0f;
		int mNumTiles = 16;
		bool mIsWireframe = false;
		bool mUseTessellatedTerrain = true;
		bool mUseNonTessellatedTerrain = true;
		bool mUseDynamicTessellation = false;
		int mTessellationFactor = 4;
		int mTessellationFactorDynamic = 64;
		int mTerrainHeightScale = 389.0f;
		float mDistanceFactor = 0.0001f;
	};
}