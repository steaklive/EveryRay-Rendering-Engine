#pragma once
#include "Common.h"
#include "GameComponent.h"
#include "GeneralEvent.h"
#include "ConstantBuffer.h"

#define MAX_FOLIAGE_ZONES 4096

namespace Library
{
	class ER_Scene;
	class Camera;
	class DirectionalLight;
	class ER_ShadowMapper;
	class ER_PostProcessingStack;
	class ER_Illumination;
	class ER_RenderableAABB;
	class ER_Terrain;

	namespace FoliageCBufferData {
		struct FoliageData {
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
		TO_GBUFFER,
		TO_VOXELIZATION
	};
	enum FoliageBillboardType
	{
		SINGLE = 0,
		TWO_QUADS_CROSSING = 1,
		THREE_QUADS_CROSSING = 2,
		MULTIPLE_QUADS_CROSSING = 3
	};

	struct GPUFoliagePatchData //for GPU vertex buffer
	{
		XMFLOAT4 pos;
		XMFLOAT2 uv;
		XMFLOAT3 normals;
	};

	struct GPUFoliageInstanceData //for GPU instance buffer
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
		ER_Foliage(Game& pGame, Camera& pCamera, DirectionalLight& pLight, int pPatchesCount, const std::string& textureName, float scale = 1.0f, float distributionRadius = 100, 
			const XMFLOAT3& distributionCenter = XMFLOAT3(0.0f, 0.0f, 0.0f), FoliageBillboardType bType = FoliageBillboardType::SINGLE,
			bool isPlacedOnTerrain = false, int terrainPlaceChannel = 4);
		~ER_Foliage();

		void Initialize();
		void Draw(const GameTime& gameTime, const ER_ShadowMapper* worldShadowMapper, FoliageRenderingPass renderPass);
		void DrawDebugGizmos();
		void Update(const GameTime& gameTime);

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

		void SetVoxelizationParams(float* worldVoxelScale, const float* voxelTexDimension, XMFLOAT4* voxelCameraPos)
		{
			mWorldVoxelScale = worldVoxelScale;
			mVoxelCameraPos = voxelCameraPos;
			mVoxelTextureDimension = voxelTexDimension;
		}

		bool PerformCPUFrustumCulling(Camera* camera);

		void SetName(const std::string& name) { mName = name; }
		const std::string& GetName() { return mName; }

		bool IsSelectedInEditor() { return mIsSelectedInEditor; }
	private:
		void InitializeBuffersGPU(int count);
		void InitializeBuffersCPU();
		void LoadBillboardModel(FoliageBillboardType bType);
		void CalculateDynamicLOD(float distanceToCam);
		void CreateBlendStates();

		Game& mGame;
		Camera& mCamera;
		DirectionalLight& mDirectionalLight;

		ID3D11InputLayout* mInputLayout = nullptr;
		ID3D11VertexShader* mVS = nullptr;
		ID3D11GeometryShader* mGS = nullptr;
		ID3D11PixelShader* mPS = nullptr;
		ID3D11PixelShader* mPS_GBuffer = nullptr;
		ID3D11PixelShader* mPS_Voxelization = nullptr;
		ConstantBuffer<FoliageCBufferData::FoliageData> mFoliageConstantBuffer;

		ID3D11Buffer* mVertexBuffer = nullptr;
		ID3D11Buffer* mIndexBuffer = nullptr;
		ID3D11Buffer* mInstanceBuffer = nullptr;
		ID3D11ShaderResourceView* mAlbedoTexture = nullptr;
		ID3D11BlendState* mAlphaToCoverageState = nullptr;
		ID3D11BlendState* mNoBlendState = nullptr;

		ID3D11UnorderedAccessView* mVoxelizationTexture = nullptr;

		GPUFoliageInstanceData* mPatchesBufferGPU = nullptr;
		CPUFoliageData* mPatchesBufferCPU = nullptr;
		XMFLOAT4* mCurrentPositions = nullptr;

		FoliageBillboardType mType;

		int mTerrainSplatChannel = 4;
		bool mIsPlacedOnTerrain = false;

		ER_RenderableAABB* mDebugGizmoAABB = nullptr;
		ER_AABB mAABB;
		const float mAABBExtentY = 25.0f;
		const float mAABBExtentXZ = 1.0f;

		std::string mName;
		std::string mTextureName;

		bool mIsSelectedInEditor = false;
		bool mIsCulled = false;

		int mPatchesCount = 0;
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

	class ER_FoliageManager : public GameComponent
	{
	public:
		ER_FoliageManager(Game& pGame, ER_Scene* aScene, DirectionalLight& light);
		~ER_FoliageManager();

		void Initialize();
		void Update(const GameTime& gameTime, float gustDistance, float strength, float frequency);
		void Draw(const GameTime& gameTime, const ER_ShadowMapper* worldShadowMapper, FoliageRenderingPass renderPass);
		void DrawDebugGizmos();
		void AddFoliage(ER_Foliage* foliage);
		void SetVoxelizationParams(float* scale, const float* dimensions, XMFLOAT4* voxelCamera);
		void Config() { mShowDebug = !mShowDebug; }

		using Delegate_FoliageSystemInitialized = std::function<void()>;
		GeneralEvent<Delegate_FoliageSystemInitialized>* FoliageSystemInitializedEvent = new GeneralEvent<Delegate_FoliageSystemInitialized>();
	private:
		void UpdateImGui();
		std::vector<ER_Foliage*> mFoliageCollection;
		ER_Scene* mScene;

		const char* mFoliageZonesNamesUI[MAX_FOLIAGE_ZONES];

		int mEditorSelectedFoliageZoneIndex = 0;

		float mMaxDistanceToCamera = 300.0f; // more than this => culled completely
		float mDeltaDistanceToCamera = 30.0f; // from which distance we start dynamic culling (patches)

		bool mShowDebug = false;
		bool mEnabled = true;
		bool mEnableCulling = true;
	};
}