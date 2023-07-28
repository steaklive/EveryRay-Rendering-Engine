#pragma once
#include "Common.h"
#include "ER_CoreComponent.h"
#include "ER_GenericEvent.h"
#include "RHI/ER_RHI.h"

#define MAX_FOLIAGE_ZONES 4096

// Minimum amount of drawn patches/instances before we start applying graphics config's "quality" factor.
// In other words, for example, our foliage zone has N patches to render (after culling or without it).
// If N > MIN_FOLIAGE_PATCHES_QUALITY_THRESHOLD, then we apply a "quality" factor that will reduce the amount of patches.
// If N <= MIN_FOLIAGE_PATCHES_QUALITY_THRESHOLD, then we assume that any graphics config can handle that amount of geometry.
#define MIN_FOLIAGE_PATCHES_QUALITY_THRESHOLD 1000

namespace EveryRay_Core
{
	class ER_Scene;
	class ER_Camera;
	class ER_DirectionalLight;
	class ER_ShadowMapper;
	class ER_PostProcessingStack;
	class ER_Illumination;
	class ER_RenderableAABB;
	class ER_Terrain;

	namespace FoliageCBufferData {
		struct ER_ALIGN_GPU_BUFFER FoliageCB {
			XMMATRIX ShadowMatrices[NUM_SHADOW_CASCADES];
			XMMATRIX World;
			XMMATRIX View;
			XMMATRIX Projection;
			XMFLOAT4 SunDirection;
			XMFLOAT4 SunColor;
			XMFLOAT4 AmbientColor;
			XMFLOAT4 ShadowTexelSize;
			XMFLOAT4 ShadowCascadeDistances;
			XMFLOAT4 CameraDirection;
			XMFLOAT4 CameraPos;
			XMFLOAT4 VoxelCameraPos;
			XMFLOAT4 WindDirection;
			float RotateToCamera;
			float Time;
			float WindFrequency;
			float WindStrength;
			float WindGustDistance;
			float WorldVoxelScale;
			float VoxelTextureDimension;
		};
	}

	enum FoliageRenderingPass
	{
		FOLIAGE_GBUFFER,
		FOLIAGE_VOXELIZATION
	};
	enum FoliageBillboardType
	{
		SINGLE = 0,
		TWO_QUADS_CROSSING = 1,
		THREE_QUADS_CROSSING = 2,
		MULTIPLE_QUADS_CROSSING = 3
	};
	enum FoliageQuality
	{
		FOLIAGE_ULTRA_LOW = 0,
		FOLIAGE_LOW,
		FOLIAGE_MEDIUM,
		FOLIAGE_HIGH
	};

	struct ER_ALIGN_GPU_BUFFER GPUFoliagePatchData //for GPU vertex buffer
	{
		XMFLOAT4 pos;
		XMFLOAT2 uv;
		XMFLOAT3 normals;
	};

	struct ER_ALIGN_GPU_BUFFER GPUFoliageInstanceData //for GPU instance buffer
	{
		XMMATRIX worldMatrix = XMMatrixIdentity();
	};

	struct CPUFoliageData //for CPU buffer
	{
		float xPos, yPos, zPos;
		float r, g, b;
		float scale;
	};

	class ER_Foliage
	{
	public:
		ER_Foliage(ER_Core& pCore, ER_Camera& pCamera, ER_DirectionalLight& pLight, int pPatchesCount, const std::string& textureName, float scale = 1.0f, float distributionRadius = 100, 
			const XMFLOAT3& distributionCenter = XMFLOAT3(0.0f, 0.0f, 0.0f), FoliageBillboardType bType = FoliageBillboardType::SINGLE,
			bool isPlacedOnTerrain = false, int terrainPlaceChannel = 4, float placedHeightDelta = 0.0f);
		~ER_Foliage();

		void Initialize();
		void Draw(const ER_CoreTime& gameTime, const ER_ShadowMapper* worldShadowMapper, FoliageRenderingPass renderPass, 
			const std::vector<ER_RHI_GPUTexture*>& aGbufferTextures, ER_RHI_GPUTexture* aDepthTarget, ER_RHI_GPURootSignature* rs);
		void DrawDebugGizmos(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPUTexture* aDepth, ER_RHI_GPURootSignature* rs);
		void Update(const ER_CoreTime& gameTime);

		bool IsSelected() { return mIsSelectedInEditor; }
		void SetSelected(bool val) { mIsSelectedInEditor = val; }
		
		void SetWireframe(bool flag) { mIsWireframe = flag; }
		void SetDynamicLODMaxDistance(float val) { mMaxDistanceToCamera = val; }
		void SetDynamicDeltaDistanceToCamera(float val) { mDeltaDistanceToCamera = val; }

		bool IsRotating() { return mIsRotating; }
		void SetWindParams(float gustDistance, float strength, float frequency) 
		{
			mWindGustDistance = gustDistance;
			mWindStrength = strength;
			mWindFrequency = frequency;
		}

		int GetPatchesCount() { return mPatchesCount; }
		void SetPatchPosition(int i, float x, float y, float z) {
			mPatchesBufferCPU[i].xPos = x;
			mPatchesBufferCPU[i].yPos = y;
			mPatchesBufferCPU[i].zPos = z;
		}
		float GetPatchPositionX(int i) { return mPatchesBufferCPU[i].xPos; }
		float GetPatchPositionY(int i) { return mPatchesBufferCPU[i].yPos; }
		float GetPatchPositionZ(int i) { return mPatchesBufferCPU[i].zPos; }
		const XMFLOAT3& GetDistributionCenter() { return mDistributionCenter; }

		void UpdateBuffersGPU();
		void UpdateBuffersCPU();
		void UpdateAABB();

		void SetVoxelizationParams(float* worldVoxelScale, const float* voxelTexDimension, XMFLOAT4* voxelCameraPos)
		{
			mWorldVoxelScale = worldVoxelScale;
			mVoxelCameraPos = voxelCameraPos;
			mVoxelTextureDimension = voxelTexDimension;
		}

		bool PerformCPUFrustumCulling(ER_Camera* camera);

		void SetName(const std::string& name) { mName = name; mOriginalName = name; }
		const std::string& GetName() { return mName; }

		bool IsSelectedInEditor() { return mIsSelectedInEditor; }
		bool IsCulled() { return mIsCulled; }
	private:
		void PrepareRendering(const ER_CoreTime& gameTime, const ER_ShadowMapper* worldShadowMapper, ER_RHI_GPURootSignature* rs);
		void InitializeBuffersGPU(int count);
		void InitializeBuffersCPU();
		void LoadBillboardModel(FoliageBillboardType bType);
		void CalculateDynamicLOD(float distanceToCam);

		ER_Core& mCore;
		ER_Camera& mCamera;
		ER_DirectionalLight& mDirectionalLight;

		ER_RHI_InputLayout* mInputLayout = nullptr;
		ER_RHI_GPUShader* mVS = nullptr;
		ER_RHI_GPUShader* mGS = nullptr;
		ER_RHI_GPUShader* mPS = nullptr;
		std::string mFoliageMainPassPSOName = "ER_RHI_GPUPipelineStateObject: Foliage - Main Pass";

		ER_RHI_GPUShader* mPS_GBuffer = nullptr;
		std::string mFoliageGBufferPassPSOName = "ER_RHI_GPUPipelineStateObject: Foliage - Gbuffer Pass";

		ER_RHI_GPUShader* mPS_Voxelization = nullptr;
		std::string mFoliageVoxelizationPassPSOName = "ER_RHI_GPUPipelineStateObject: Foliage - Voxelization Pass";

		ER_RHI_GPUConstantBuffer<FoliageCBufferData::FoliageCB> mFoliageConstantBuffer;

		ER_RHI_GPUBuffer* mVertexBuffer = nullptr;
		ER_RHI_GPUBuffer* mIndexBuffer = nullptr;
		ER_RHI_GPUBuffer* mInstanceBuffer = nullptr;
		ER_RHI_GPUTexture* mAlbedoTexture = nullptr;
		ER_RHI_GPUTexture* mVoxelizationTexture = nullptr;

		GPUFoliageInstanceData* mPatchesBufferGPU = nullptr;
		CPUFoliageData* mPatchesBufferCPU = nullptr;
		XMFLOAT4* mCurrentPositions = nullptr;

		ER_RHI_GPUBuffer* mInputPositionsOnTerrainBuffer = nullptr; //input positions for on-terrain placement pass
		ER_RHI_GPUBuffer* mOutputPositionsOnTerrainBuffer = nullptr; //output positions for on-terrain placement pass

		FoliageBillboardType mType;

		int mTerrainSplatChannel = 4;
		bool mIsPlacedOnTerrain = false;
		float mPlacementHeightDelta = 0.0;

		ER_RenderableAABB* mDebugGizmoAABB = nullptr;
		ER_AABB mAABB;
		const float mAABBExtentY = 25.0f;
		const float mAABBExtentXZ = 1.0f;

		std::string mName;
		std::string mOriginalName; //unchanged
		std::string mTextureName;

		bool mIsSelectedInEditor = false;
		bool mIsCulled = false;

		int mPatchesCount = 0; // original patches count (unchanged)
		int mPatchesCountToRender = 0;

		float mMaxDistanceToCamera = 0.0f;
		float mDeltaDistanceToCamera = 0.0f;

		bool mIsWireframe = false;
		float mScale;
		XMFLOAT3 mDistributionCenter;
		float mDistributionRadius;
		bool mRotateFromCamPosition = false;

		int mVerticesCount = 0;
		bool mIsRotating = false;

		float mWindStrength;
		float mWindFrequency;
		float mWindGustDistance;

		float* mWorldVoxelScale;
		const float* mVoxelTextureDimension;
		XMFLOAT4* mVoxelCameraPos;

		float		mCameraViewMatrix[16];
		float		mCameraProjectionMatrix[16];
		XMMATRIX	mTransformationMatrix;
		float		mMatrixTranslation[3];
		float		mMatrixRotation[3];
		float		mMatrixScale[3];
		float		mCurrentObjectTransformMatrix[16] =
		{
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 1.f
		};
	};

	class ER_FoliageManager : public ER_CoreComponent
	{
	public:
		ER_FoliageManager(ER_Core& pCore, ER_Scene* aScene, ER_DirectionalLight& light, FoliageQuality aQuality = FoliageQuality::FOLIAGE_HIGH);
		~ER_FoliageManager();

		void Initialize();
		void Update(const ER_CoreTime& gameTime, float gustDistance, float strength, float frequency);
		void Draw(const ER_CoreTime& gameTime, const ER_ShadowMapper* worldShadowMapper, FoliageRenderingPass renderPass,
			const std::vector<ER_RHI_GPUTexture*>& aGbufferTextures, ER_RHI_GPUTexture* aDepthTarget = nullptr);
		void DrawDebugGizmos(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPUTexture* aDepth, ER_RHI_GPURootSignature* rs);
		void Config() { mShowDebug = !mShowDebug; }

		void AddFoliage(ER_Foliage* foliage);
		void SetVoxelizationParams(float* scale, const float* dimensions, XMFLOAT4* voxelCamera);

		float GetQualityFactor() { return mCurrentFoliageQualityFactor; }

		using Delegate_FoliageSystemInitialized = std::function<void()>;
		ER_GenericEvent<Delegate_FoliageSystemInitialized>* FoliageSystemInitializedEvent = new ER_GenericEvent<Delegate_FoliageSystemInitialized>();
	private:
		void UpdateImGui();
		std::vector<ER_Foliage*> mFoliageCollection;
		ER_Scene* mScene = nullptr;

		ER_RHI_GPURootSignature* mRootSignature = nullptr;

		FoliageQuality mCurrentFoliageQuality = FoliageQuality::FOLIAGE_HIGH;
		float mCurrentFoliageQualityFactor = 1.0f; // percentage of drawn foliage patches/instances based on quality preset (only active when > MIN_FOLIAGE_PATCHES_QUALITY_THRESHOLD)

		const char* mFoliageZonesNamesUI[MAX_FOLIAGE_ZONES];

		int mEditorSelectedFoliageZoneIndex = 0;

		float mMaxDistanceToCamera = 300.0f; // more than this => culled completely
		float mDeltaDistanceToCamera = 30.0f; // from which distance we start dynamic culling (patches)

		bool mShowDebug = false;
		bool mEnabled = true;
		bool mEnableCulling = true;
	};
}