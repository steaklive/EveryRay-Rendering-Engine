#pragma once
#include "Common.h"
#include "GameComponent.h"
#include "Camera.h"
#include "DirectionalLight.h"
#include "ER_PostProcessingStack.h"
#include "ConstantBuffer.h"
#include "ER_GPUTexture.h"

#define NUM_THREADS_PER_TERRAIN_SIDE 4
#define NUM_TERRAIN_PATCHES_PER_TILE 8
#define NUM_TEXTURE_SPLAT_CHANNELS 4
#define TERRAIN_TILE_RESOLUTION 512 //texture res. of one terrain tile (based on heightmap)

namespace Library 
{
	class ER_ShadowMapper;
	class ER_Scene;
	class GameTime;
	class DirectionalLight;

	enum TerrainSplatChannels {
		CHANNEL_0,
		CHANNEL_1,
		CHANNEL_2,
		CHANNEL_3
	};

	namespace TerrainCBufferData {
		struct TerrainData {
			XMMATRIX ShadowMatrices[NUM_SHADOW_CASCADES];
			XMMATRIX World;
			XMMATRIX View;
			XMMATRIX Projection;
			XMFLOAT4 SunDirection;
			XMFLOAT4 SunColor;
			XMFLOAT4 ShadowTexelSize;
			XMFLOAT4 ShadowCascadeDistances;
			XMFLOAT4 CameraPosition;
			float TessellationFactor;
			float TerrainHeightScale;
			float UseDynamicTessellation;
			float TessellationFactorDynamic;
			float DistanceFactor;
		};
	}

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

		Vertex mVertexList[(TERRAIN_TILE_RESOLUTION - 1) * (TERRAIN_TILE_RESOLUTION - 1) * 6];
		MapData* mData = nullptr;

		ID3D11Buffer* mVertexBufferTS = nullptr;
		ER_GPUTexture* mSplatTexture = nullptr;
		ER_GPUTexture* mHeightTexture = nullptr;
		XMMATRIX mWorldMatrixTS = XMMatrixIdentity();
		int mVertexCount = 0; //not used in GPU tessellated terrain

		XMFLOAT2 mUVOffsetToTextureSpace;

	};

	class Terrain : public GameComponent
	{
	public:
		Terrain(Game& game, DirectionalLight& light);
		~Terrain();

		void LoadTerrainData(ER_Scene* aScene);

		UINT GetWidth() { return mWidth; }
		UINT GetHeight() { return mHeight; }

		void Draw(ER_ShadowMapper* worldShadowMapper = nullptr);
		void Update(const GameTime& gameTime);
		void Config() { mShowDebug = !mShowDebug; }
		
		void SetLevelPath(const std::wstring& aPath) { mLevelPath = aPath; };

		void SetWireframeMode(bool flag) { mIsWireframe = flag; }
		//void SetTessellationTerrainMode(bool flag) { mUseTessellatedTerrain = flag; }
		//void SetNormalTerrainMode(bool flag) { mUseNonTessellatedTerrain = flag; }
		void SetDynamicTessellation(bool flag) { mUseDynamicTessellation = flag; }
		void SetTessellationFactor(int tessellationFactor) { mTessellationFactor = tessellationFactor; }
		void SetDynamicTessellationDistanceFactor(float factor) { mTessellationDistanceFactor = factor; }
		void SetTessellationFactorDynamic(float factor) { mTessellationFactorDynamic = factor; }
		void SetTerrainHeightScale(float scale) { mTerrainTessellatedHeightScale = scale; }
		HeightMap* GetHeightmap(int index) { return mHeightMaps.at(index); }
		//float GetHeightScale(bool tessellated) { if (tessellated) return mTerrainTessellatedHeightScale; else return mTerrainNonTessellatedHeightScale; }
		
		void SetEnabled(bool val) { mEnabled = val; }
		bool IsEnabled() { return mEnabled; }
	private:
		void LoadTextures(const std::wstring& aTexturesPath, const std::wstring& splatLayer0Path, const std::wstring& splatLayer1Path,	const std::wstring& splatLayer2Path, const std::wstring& splatLayer3Path);

		void LoadTile(int threadIndex, const std::wstring& path);
		void GenerateTileMesh(int tileIndex);
		void LoadRawHeightmapPerTileCPU(int tileIndexX, int tileIndexY, const std::wstring& aPath);
		void LoadSplatmapPerTileGPU(int tileIndexX, int tileIndexY, const std::wstring& path);
		void LoadHeightmapPerTileGPU(int tileIndexX, int tileIndexY, const std::wstring& path);
		void LoadNormalmapPerTileGPU(int tileIndexX, int tileIndexY, const std::wstring& path);
		void DrawTessellated(int i, ER_ShadowMapper* worldShadowMapper = nullptr);
		void DrawNonTessellated(int i, ER_ShadowMapper* worldShadowMapper = nullptr);

		DirectionalLight& mDirectionalLight;

		ConstantBuffer<TerrainCBufferData::TerrainData> mTerrainConstantBuffer;

		ID3D11InputLayout* mInputLayout = nullptr;

		ID3D11VertexShader* mVS = nullptr;
		ID3D11HullShader* mHS = nullptr;
		ID3D11DomainShader* mDS = nullptr;
		ID3D11PixelShader* mPS = nullptr;
		
		std::vector<HeightMap*> mHeightMaps;
		ER_GPUTexture* mSplatChannelTextures[NUM_TEXTURE_SPLAT_CHANNELS];

		std::wstring mLevelPath;

		UINT mWidth = 0;
		UINT mHeight = 0;

		int mNumTiles = 0;
		float mTerrainTessellatedHeightScale = 328.0f;
		bool mIsWireframe = false;
		bool mUseDynamicTessellation = true;
		int mTessellationFactor = 4;
		int mTessellationFactorDynamic = 64;
		float mTessellationDistanceFactor = 0.015f;

		bool mShowDebug = false;
		bool mEnabled = true;
	};
}