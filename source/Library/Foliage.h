#pragma once
#include "Common.h"
#include "GameComponent.h"
#include "Camera.h"
#include "DirectionalLight.h"
#include "ER_PostProcessingStack.h"
#include "FoliageMaterial.h"
#include "ER_ShadowMapper.h"
#include "Illumination.h"
#include "GeneralEvent.h"

namespace Library
{
	enum FoliageRenderingPass
	{
		FORWARD_SHADING,
		TO_GBUFFER,
		VOXELIZATION
	};
	enum FoliageBillboardType
	{
		SINGLE,
		TWO_QUADS_CROSSING,
		THREE_QUADS_CROSSING
	};

	struct FoliagePatchData //for GPU vertex buffer
	{
		XMFLOAT4 pos;
		XMFLOAT2 uv;
		XMFLOAT3 normals;
	};

	struct FoliageInstanceData //for GPU instance buffer
	{
		XMMATRIX worldMatrix = XMMatrixIdentity();
	};

	struct FoliageData //for CPU buffer
	{
		float xPos, yPos, zPos;
		float r, g, b;
		float scale;
	};

	class Foliage : public GameComponent
	{

	public:
		Foliage(Game& pGame, Camera& pCamera, DirectionalLight& pLight, int pPatchesCount, std::string textureName, float scale = 1.0f, float distributionRadius = 100, XMFLOAT3 distributionCenter = XMFLOAT3(0.0f, 0.0f, 0.0f), FoliageBillboardType bType = FoliageBillboardType::SINGLE);
		~Foliage();

		void Initialize();
		void Draw(const GameTime& gameTime, const ER_ShadowMapper* worldShadowMapper = nullptr, FoliageRenderingPass renderPass = FoliageRenderingPass::FORWARD_SHADING);
		void Update(const GameTime& gameTime);

		int GetPatchesCount() { return mPatchesCount; }
		void SetWireframe(bool flag) { mIsWireframe = flag; }
		void SetDynamicLODMaxDistance(float val) { mMaxDistanceToCamera = val; }
		bool IsRotating() { return mIsRotating; }
		void SetWindParams(float gustDistance, float strength, float frequency) 
		{
			mWindGustDistance = gustDistance;
			mWindStrength = strength;
			mWindFrequency = frequency;
		}

		void SetPatchPosition(int i, float x, float y, float z) {
			mPatchesBufferCPU[i].xPos = x;
			mPatchesBufferCPU[i].yPos = y;
			mPatchesBufferCPU[i].zPos = z;
		}

		float GetPatchPositionX(int i) { return mPatchesBufferCPU[i].xPos; }
		float GetPatchPositionZ(int i) { return mPatchesBufferCPU[i].zPos; }

		void CreateBufferGPU();
		void UpdateBuffersGPU();

		void SetVoxelizationTextureOutput(ID3D11UnorderedAccessView* uav) { mVoxelizationTexture = uav; }
		void SetVoxelizationParams(float* worldVoxelScale, XMFLOAT4* voxelCameraPos)
		{
			mWorldVoxelScale = worldVoxelScale;
			mVoxelCameraPos = voxelCameraPos;
		}
	private:
		void InitializeBuffersGPU(int count);
		void InitializeBuffersCPU();
		void LoadBillboardModel(FoliageBillboardType bType);
		void CalculateDynamicLOD(float distanceToCam);
		void CreateBlendStates();

		Camera& mCamera;
		DirectionalLight& mDirectionalLight;

		FoliageMaterial* mFoliageMaterial = nullptr;

		ID3D11Buffer* mVertexBuffer = nullptr;
		ID3D11Buffer* mIndexBuffer = nullptr;
		ID3D11Buffer* mInstanceBuffer = nullptr;
		ID3D11ShaderResourceView* mAlbedoTexture = nullptr;
		ID3D11BlendState* mAlphaToCoverageState = nullptr;
		ID3D11BlendState* mNoBlendState = nullptr;

		ID3D11UnorderedAccessView* mVoxelizationTexture = nullptr;

		FoliageInstanceData* mPatchesBufferGPU = nullptr;
		FoliageData* mPatchesBufferCPU = nullptr;

		FoliageBillboardType mType;

		int mPatchesCount;
		int mPatchesCountVisible;
		int mPatchesCountToRender;

		bool mIsWireframe = false;
		float mScale;
		XMFLOAT3 mDistributionCenter;
		float mDistributionRadius;
		bool mRotateFromCamPosition = false;

		bool mDynamicLOD = true;
		float mDynamicLODFactor = 0.001;
		float mDoRotationDistance = 300.0f;
		float mMaxDistanceToCamera = 650.0f;

		int mVerticesCount = 0;
		bool mIsRotating = false;

		float mWindStrength;
		float mWindFrequency;
		float mWindGustDistance;

		float* mWorldVoxelScale;
		XMFLOAT4* mVoxelCameraPos;
	};

	class FoliageSystem
	{
	public:
		FoliageSystem();
		FoliageSystem(const FoliageSystem& rhs);
		FoliageSystem& operator=(const FoliageSystem& rhs);
		~FoliageSystem();

		void Initialize();
		void Update(const GameTime& gameTime, float gustDistance, float strength, float frequency);
		void Draw(const GameTime& gameTime, const ER_ShadowMapper* worldShadowMapper, FoliageRenderingPass renderPass = FoliageRenderingPass::FORWARD_SHADING);
		void AddFoliage(Foliage* foliage) { mFoliageCollection.emplace_back(foliage); }
		void SetVoxelizationTextureOutput(ID3D11UnorderedAccessView* uav);
		void SetVoxelizationParams(float* scale, XMFLOAT4* voxelCamera);

		using Delegate_FoliageSystemInitialized = std::function<void()>;
		GeneralEvent<Delegate_FoliageSystemInitialized>* FoliageSystemInitializedEvent = new GeneralEvent<Delegate_FoliageSystemInitialized>();
	private:
		std::vector<Foliage*> mFoliageCollection;
	};
}