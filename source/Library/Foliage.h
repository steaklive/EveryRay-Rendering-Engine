#pragma once
#include "Common.h"
#include "GameComponent.h"
#include "GeneralEvent.h"
#include "ConstantBuffer.h"

namespace Library
{
	class Scene;
	class Camera;
	class DirectionalLight;
	class ER_ShadowMapper;
	class ER_PostProcessingStack;
	class Illumination;

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
		THREE_QUADS_CROSSING = 2
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

	class Foliage : public GameComponent
	{

	public:
		Foliage(Game& pGame, Camera& pCamera, DirectionalLight& pLight, int pPatchesCount, std::string textureName, float scale = 1.0f, float distributionRadius = 100, const XMFLOAT3& distributionCenter = XMFLOAT3(0.0f, 0.0f, 0.0f), FoliageBillboardType bType = FoliageBillboardType::SINGLE);
		~Foliage();

		void Initialize();
		void Draw(const GameTime& gameTime, const ER_ShadowMapper* worldShadowMapper, FoliageRenderingPass renderPass);
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

		void SetVoxelizationParams(float* worldVoxelScale, const float* voxelTexDimension, XMFLOAT4* voxelCameraPos)
		{
			mWorldVoxelScale = worldVoxelScale;
			mVoxelCameraPos = voxelCameraPos;
			mVoxelTextureDimension = voxelTexDimension;
		}
	private:
		void InitializeBuffersGPU(int count);
		void InitializeBuffersCPU();
		void LoadBillboardModel(FoliageBillboardType bType);
		void CalculateDynamicLOD(float distanceToCam);
		void CreateBlendStates();

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
		const float* mVoxelTextureDimension;
		XMFLOAT4* mVoxelCameraPos;
	};

	class FoliageSystem
	{
	public:
		FoliageSystem(Scene* aScene, DirectionalLight& light);
		~FoliageSystem();

		void Initialize();
		void Update(const GameTime& gameTime, float gustDistance, float strength, float frequency);
		void Draw(const GameTime& gameTime, const ER_ShadowMapper* worldShadowMapper, FoliageRenderingPass renderPass);
		void AddFoliage(Foliage* foliage);
		void SetVoxelizationParams(float* scale, const float* dimensions, XMFLOAT4* voxelCamera);

		using Delegate_FoliageSystemInitialized = std::function<void()>;
		GeneralEvent<Delegate_FoliageSystemInitialized>* FoliageSystemInitializedEvent = new GeneralEvent<Delegate_FoliageSystemInitialized>();
	private:
		std::vector<Foliage*> mFoliageCollection;
	};
}