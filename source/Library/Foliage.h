#pragma once
#include "Common.h"
#include "GameComponent.h"
#include "Camera.h"
#include "DirectionalLight.h"
#include "PostProcessingStack.h"
#include "FoliageMaterial.h"
#include "ShadowMapper.h"

namespace Library
{
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
	};

	struct FoliageInstanceData //for GPU instance buffer
	{
		XMMATRIX worldMatrix = XMMatrixIdentity();
		XMFLOAT3 color;
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
		void Draw(const GameTime& gameTime, ShadowMapper* worldShadowMapper = nullptr);
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
	private:
		void InitializeBuffersGPU(int count);
		void InitializeBuffersCPU();
		void LoadBillboardModel(FoliageBillboardType bType);
		void CalculateDynamicLOD(float distanceToCam);
		void CreateBlendStates();

		Camera& mCamera;
		DirectionalLight& mDirectionalLight;

		FoliageMaterial* mMaterial;

		ID3D11Buffer* mVertexBuffer = nullptr;
		ID3D11Buffer* mIndexBuffer = nullptr;
		ID3D11Buffer* mInstanceBuffer = nullptr;
		ID3D11ShaderResourceView* mAlbedoTexture = nullptr;
		ID3D11BlendState* mAlphaToCoverageState = nullptr;
		ID3D11BlendState* mNoBlendState = nullptr;

		FoliageInstanceData* mPatchesBufferGPU;
		FoliageData* mPatchesBufferCPU;

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
	};
}