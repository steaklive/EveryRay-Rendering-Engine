#pragma once
#include "Common.h"
#include "ER_CoreComponent.h"
#include "ER_GenericEvent.h"
#include "RHI/ER_RHI.h"

#define NUM_THREADS_PER_TERRAIN_SIDE 4
#define NUM_TERRAIN_PATCHES_PER_TILE 8
#define NUM_TEXTURE_SPLAT_CHANNELS 4

namespace EveryRay_Core 
{
	static char* DisplayedSplatChannnelNames[5] = {
		"Channel 0",
		"Channel 1",
		"Channel 2",
		"Channel 3",
		"NONE"
	};

	class ER_ShadowMapper;
	class ER_Scene;
	class ER_CoreTime;
	class ER_DirectionalLight;
	class ER_LightProbesManager;
	class ER_RenderableAABB;
	class ER_Camera;

	struct ER_ALIGN_GPU_BUFFER TerrainTileDataGPU
	{
		XMFLOAT4 UVoffsetTileSize; // x,y - offsets, z,w - tile size
		XMFLOAT4 AABBMinPoint;
		XMFLOAT4 AABBMaxPoint;
	};

	enum TerrainSplatChannels {
		CHANNEL_0 = 0,
		CHANNEL_1 = 1,
		CHANNEL_2 = 2,
		CHANNEL_3 = 3,
		NONE = 4
	};

	enum TerrainRenderPass
	{
		TERRAIN_GBUFFER,
		TERRAIN_FORWARD,
		TERRAIN_SHADOW
	};

	namespace TerrainCBufferData {
		struct ER_ALIGN_GPU_BUFFER TerrainShadowCB {
			XMMATRIX LightViewProjection;
		};

		struct ER_ALIGN_GPU_BUFFER TerrainCB {
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
			float TileSize;
		};

		struct ER_ALIGN_GPU_BUFFER PlaceOnTerrainData
		{
			float HeightScale;
			float SplatChannel;
			float TerrainTileCount;
			float PlacementHeightDelta;
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
		bool PerformCPUFrustumCulling(ER_Camera* camera);
		bool IsCulled() { return mIsCulled; }
		bool IsColliding(const XMFLOAT4& position, bool onlyXZCheck = false);

		HeightMap(int width, int height);
		~HeightMap();

		Vertex* mVertexList = nullptr;
		MapData* mData = nullptr;

		ER_RHI_GPUTexture* mSplatTexture = nullptr;
		ER_RHI_GPUTexture* mHeightTexture = nullptr;

		ER_RenderableAABB* mDebugGizmoAABB = nullptr;
		ER_AABB mAABB; //based on the CPU terrain

		XMFLOAT2 mTileUVOffset = XMFLOAT2(0.0, 0.0);

		ER_RHI_GPUBuffer* mVertexBufferTS = nullptr;
		XMMATRIX mWorldMatrixTS = XMMatrixIdentity();

		ER_RHI_GPUBuffer* mVertexBufferNonTS = nullptr;
		int mVertexCountNonTS = 0; //not used in GPU tessellated terrain
		ER_RHI_GPUBuffer* mIndexBufferNonTS = nullptr;
		int mIndexCountNonTS = 0; //not used in GPU tessellated terrain

		bool mIsCulled = false;
	};

	class ER_Terrain : public ER_CoreComponent
	{
	public:
		ER_Terrain(ER_Core& game, ER_DirectionalLight& light);
		~ER_Terrain();

		void LoadTerrainData(ER_Scene* aScene);

		UINT GetWidth() { return mWidth; }
		UINT GetHeight() { return mHeight; }

		void Draw(TerrainRenderPass aPass, const std::vector<ER_RHI_GPUTexture*>& aRenderTargets, ER_RHI_GPUTexture* aDepthTarget = nullptr, ER_ShadowMapper* worldShadowMapper = nullptr, ER_LightProbesManager* probeManager = nullptr, int shadowMapCascade = -1);
		void DrawDebugGizmos(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPURootSignature* rs);
		void Update(const ER_CoreTime& gameTime);
		void Config() { mShowDebug = !mShowDebug; }
		
		void SetLevelPath(const std::wstring& aPath) { mLevelPath = aPath; };

		void SetWireframeMode(bool flag) { mIsWireframe = flag; }
		void SetDynamicTessellation(bool flag) { mUseDynamicTessellation = flag; }
		void SetTessellationFactor(int tessellationFactor) { mTessellationFactor = tessellationFactor; }
		void SetDynamicTessellationDistanceFactor(float factor) { mTessellationDistanceFactor = factor; }
		void SetTessellationFactorDynamic(float factor) { mTessellationFactorDynamic = factor; }
		void SetTerrainHeightScale(float scale) { mTerrainTessellatedHeightScale = scale; }
		HeightMap* GetHeightmap(int index) { return mHeightMaps.at(index); }
		void PlaceOnTerrain(ER_RHI_GPUBuffer* outputBuffer, ER_RHI_GPUBuffer* inputBuffer, XMFLOAT4* positions, int positionsCount,
			TerrainSplatChannels splatChannel = TerrainSplatChannels::NONE,	XMFLOAT4* terrainVertices = nullptr, int terrainVertexCount = 0);
		void ReadbackPlacedPositions(ER_RHI_GPUBuffer* outputBuffer, ER_RHI_GPUBuffer* inputBuffer, XMFLOAT4* positions, int positionsCount);
		//float GetHeightScale(bool tessellated) { if (tessellated) return mTerrainTessellatedHeightScale; else return mTerrainNonTessellatedHeightScale; }

		void SetEnabled(bool val) { mEnabled = val; }
		bool IsEnabled() { return mEnabled; }
		bool IsLoaded() { return mLoaded; }

		using Delegate_ReadbackPlacedPositions = std::function<void(ER_Terrain* aTerrain)>;
		ER_GenericEvent<Delegate_ReadbackPlacedPositions>* ReadbackPlacedPositionsOnInitEvent = new ER_GenericEvent<Delegate_ReadbackPlacedPositions>();
		ER_GenericEvent<Delegate_ReadbackPlacedPositions>* ReadbackPlacedPositionsOnUpdateEvent = new ER_GenericEvent<Delegate_ReadbackPlacedPositions>();
	private:
		void LoadTile(int threadIndex, const std::wstring& path);
		void CreateTerrainTileDataCPU(int tileIndexX, int tileIndexY, const std::wstring& aPath);
		void CreateTerrainTileDataGPU(int tileIndexX, int tileIndexY);
		void LoadTextures(const std::wstring& aTexturesPath, const std::wstring& splatLayer0Path, const std::wstring& splatLayer1Path,	const std::wstring& splatLayer2Path, const std::wstring& splatLayer3Path);
		void LoadSplatmapPerTileGPU(int tileIndexX, int tileIndexY, const std::wstring& path);
		void LoadHeightmapPerTileGPU(int tileIndexX, int tileIndexY, const std::wstring& path);
		void DrawTessellated(TerrainRenderPass aPass, const std::vector<ER_RHI_GPUTexture*>& aRenderTargets, ER_RHI_GPUTexture* aDepthTarget, int i, ER_ShadowMapper* worldShadowMapper = nullptr, ER_LightProbesManager* probeManager = nullptr, int shadowMapCascade = -1);

		ER_DirectionalLight& mDirectionalLight;

		ER_RHI_GPUConstantBuffer<TerrainCBufferData::TerrainShadowCB> mTerrainShadowBuffers[NUM_SHADOW_CASCADES];
		std::vector<ER_RHI_GPUConstantBuffer<TerrainCBufferData::TerrainCB>> mTerrainConstantBuffers; //ugly way for dx12 (I'd better have arrays in cbuffer)
		ER_RHI_GPUConstantBuffer<TerrainCBufferData::PlaceOnTerrainData> mPlaceOnTerrainConstantBuffer;

		ER_RHI_InputLayout* mInputLayout = nullptr;

		ER_RHI_GPUShader* mVS = nullptr;
		ER_RHI_GPUShader* mHS = nullptr;
		ER_RHI_GPUShader* mDS = nullptr;
		ER_RHI_GPUShader* mPS = nullptr;
		std::string mTerrainMainPassPSOName = "Terrain - Main Pass PSO";

		ER_RHI_GPUShader* mDS_ShadowMap = nullptr;
		ER_RHI_GPUShader* mPS_ShadowMap = nullptr;
		std::string mTerrainShadowPassPSOName = "Terrain - Shadow Pass PSO";

		ER_RHI_GPUShader* mPS_GBuffer = nullptr;
		std::string mTerrainGBufferPassPSOName = "Terrain - GBuffer Pass PSO";

		ER_RHI_GPUShader* mPlaceOnTerrainCS = nullptr;
		std::string mTerrainPlacementPassPSOName = "Terrain - Placement Pass PSO";
		ER_RHI_GPURootSignature* mTerrainPlacementPassRS = nullptr;
		ER_RHI_GPURootSignature* mTerrainCommonPassRS = nullptr;

		ER_RHI_GPUBuffer* mTerrainTilesDataGPU = nullptr;
		ER_RHI_GPUTexture* mTerrainTilesHeightmapsArrayTexture = nullptr;
		ER_RHI_GPUTexture* mTerrainTilesSplatmapsArrayTexture = nullptr;

		std::vector<HeightMap*> mHeightMaps;
		ER_RHI_GPUTexture* mSplatChannelTextures[NUM_TEXTURE_SPLAT_CHANNELS] = { nullptr };

		ER_RHI_GPUBuffer* mReadbackPositionsBuffer = nullptr;
		ER_RHI_GPUBuffer* mTempPositionsBuffer = nullptr;

		std::wstring mLevelPath;

		UINT mWidth = 0;
		UINT mHeight = 0;

		int mNumTiles = 0;
		int mTileResolution = 0; //texture res. of one terrain tile (based on heightmap)
		float mTileScale = 0.0f;
		float mTerrainTessellatedHeightScale = 328.0f;
		bool mIsWireframe = false;
		bool mUseDynamicTessellation = true;
		int mTessellationFactor = 4;
		int mTessellationFactorDynamic = 64;
		float mTessellationDistanceFactor = 0.015f;
		float mPlacementHeightDelta = 0.5f; // how much we want to damp the point on terrain

		bool mDrawDebugAABBs = false;
		bool mDoCPUFrustumCulling = true;
		bool mShowDebug = false;
		bool mEnabled = true;
		bool mLoaded = false;
	};
}