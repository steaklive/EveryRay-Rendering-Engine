#pragma once
#include "Common.h"
#include "GameComponent.h"
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
	class ER_LightProbesManager;
	class ER_RenderableAABB;
	class Camera;

	enum TerrainSplatChannels {
		CHANNEL_0,
		CHANNEL_1,
		CHANNEL_2,
		CHANNEL_3
	};

	namespace TerrainCBufferData {
		struct TerrainData {
			XMMATRIX ShadowMatrices[NUM_SHADOW_CASCADES];
			XMMATRIX WorldLightViewProjection;
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
		};

		struct Vertex {
			float x, y, z;
		};

	public:
		bool GetHeightFromTriangle(float x, float z, float v0[3], float v1[3], float v2[3], float normal[3], float& height);
		bool RayIntersectsTriangle(float x, float z, float v0[3], float v1[3], float v2[3], float normals[3], float& height);
		float FindHeightFromPosition(float x, float z);
		bool PerformCPUFrustumCulling(Camera* camera);
		bool IsCulled() { return mIsCulled; }

		HeightMap(int width, int height);
		~HeightMap();

		Vertex mVertexList[(TERRAIN_TILE_RESOLUTION - 1) * (TERRAIN_TILE_RESOLUTION - 1) * 6];
		MapData* mData = nullptr;

		ER_GPUTexture* mSplatTexture = nullptr;
		ER_GPUTexture* mHeightTexture = nullptr;

		ER_RenderableAABB* mDebugGizmoAABB = nullptr;
		ER_AABB mAABB; //based on the CPU terrain

		ID3D11Buffer* mVertexBufferTS = nullptr;
		XMMATRIX mWorldMatrixTS = XMMatrixIdentity();

		ID3D11Buffer* mVertexBufferNonTS = nullptr;
		int mVertexCountNonTS = 0; //not used in GPU tessellated terrain
		ID3D11Buffer* mIndexBufferNonTS = nullptr;
		int mIndexCountNonTS = 0; //not used in GPU tessellated terrain

		bool mIsCulled = false;
	};

	class ER_Terrain : public GameComponent
	{
	public:
		ER_Terrain(Game& game, DirectionalLight& light);
		~ER_Terrain();

		void LoadTerrainData(ER_Scene* aScene);

		UINT GetWidth() { return mWidth; }
		UINT GetHeight() { return mHeight; }

		void Draw(ER_ShadowMapper* worldShadowMapper = nullptr, ER_LightProbesManager* probeManager = nullptr, int shadowMapCascade = -1);
		void DrawDebugGizmos();
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
		bool IsLoaded() { return mLoaded; }
	private:
		void LoadTile(int threadIndex, const std::wstring& path);
		void CreateTerrainTileDataCPU(int tileIndexX, int tileIndexY, const std::wstring& aPath);
		void CreateTerrainTileDataGPU(int tileIndexX, int tileIndexY);
		void LoadTextures(const std::wstring& aTexturesPath, const std::wstring& splatLayer0Path, const std::wstring& splatLayer1Path,	const std::wstring& splatLayer2Path, const std::wstring& splatLayer3Path);
		void LoadSplatmapPerTileGPU(int tileIndexX, int tileIndexY, const std::wstring& path);
		void LoadHeightmapPerTileGPU(int tileIndexX, int tileIndexY, const std::wstring& path);
		void DrawTessellated(int i, ER_ShadowMapper* worldShadowMapper = nullptr, ER_LightProbesManager* probeManager = nullptr, int shadowMapCascade = -1);

		DirectionalLight& mDirectionalLight;

		ConstantBuffer<TerrainCBufferData::TerrainData> mTerrainConstantBuffer;

		ID3D11InputLayout* mInputLayout = nullptr;

		ID3D11VertexShader* mVS = nullptr;
		ID3D11HullShader* mHS = nullptr;
		ID3D11DomainShader* mDS = nullptr;
		ID3D11DomainShader* mDS_ShadowMap = nullptr;
		ID3D11PixelShader* mPS = nullptr;
		ID3D11PixelShader* mPS_ShadowMap = nullptr;
		
		std::vector<HeightMap*> mHeightMaps;
		ER_GPUTexture* mSplatChannelTextures[NUM_TEXTURE_SPLAT_CHANNELS] = { nullptr };

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

		bool mDrawDebugAABBs = false;
		bool mDoCPUFrustumCulling = true;
		bool mShowDebug = false;
		bool mEnabled = true;
		bool mLoaded = false;
	};
}