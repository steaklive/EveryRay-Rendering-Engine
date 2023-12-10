#pragma once

#include "Common.h"
#include "ER_CoreComponent.h"
#include "RHI/ER_RHI.h"

namespace EveryRay_Core
{
	class ER_DirectionalLight;
	class ER_CoreTime;
	class ER_Camera;
	class ER_Skybox;
	class ER_ShadowMapper;

	enum VolumetricFogQuality
	{
		VF_DISABLED = 0,
		VF_MEDIUM,
		VF_HIGH
	};

	namespace VolumetricFogCBufferData {
		struct ER_ALIGN_GPU_BUFFER MainCB
		{
			XMMATRIX InvViewProj;
			XMMATRIX PrevViewProj;
			XMMATRIX ShadowMatrix;
			XMFLOAT4 SunDirection;
			XMFLOAT4 SunColor;
			XMFLOAT4 CameraPosition;
			XMFLOAT4 CameraNearFar_FrameIndex_PreviousFrameBlend;
			XMFLOAT4 VolumeSize;
			float Anisotropy;
			float Density;
			float Strength;
			float ThicknessFactor;
		};	
		struct ER_ALIGN_GPU_BUFFER CompositeCB
		{
			XMMATRIX ViewProj;
			XMFLOAT4 CameraNearFar;
			XMFLOAT4 VolumeSize;
			float BlendingWithSceneColorFactor;
		};
	}

	class ER_VolumetricFog : public ER_CoreComponent
	{
	public:
		ER_VolumetricFog(ER_Core& game, const ER_DirectionalLight& aLight, const ER_ShadowMapper& aShadowMapper, VolumetricFogQuality aQuality);
		~ER_VolumetricFog();
	
		void Initialize();
		void Draw();
		void Composite(ER_RHI_GPUTexture* aRT, ER_RHI_GPUTexture* aInputColorTexture, ER_RHI_GPUTexture* aGbufferWorldPos);
		void Update(const ER_CoreTime& gameTime);
		void Config() { mShowDebug = !mShowDebug; }
		bool IsEnabled() {	return mEnabled && (mCurrentQuality != VolumetricFogQuality::VF_DISABLED);	}
		void SetEnabled(bool val) { mEnabled = val; }

		ER_RHI_GPUTexture* GetVoxelFogTexture() { return mFinalVoxelAccumulationTexture3D; }
	private:
		void ComputeInjection();
		void ComputeAccumulation();
		void UpdateImGui();

		ER_Camera* mCameraFog = nullptr;

		const ER_ShadowMapper& mShadowMapper;
		const ER_DirectionalLight& mDirectionalLight;

		ER_RHI_GPUTexture* mTempVoxelInjectionTexture3D[2] = { nullptr, nullptr }; //read-write
		ER_RHI_GPUTexture* mFinalVoxelAccumulationTexture3D = nullptr;
		ER_RHI_GPUTexture* mBlueNoiseTexture = nullptr;

		ER_RHI_GPUConstantBuffer<VolumetricFogCBufferData::MainCB> mMainConstantBuffer;
		ER_RHI_GPUConstantBuffer<VolumetricFogCBufferData::CompositeCB> mCompositeConstantBuffer;

		ER_RHI_GPURootSignature* mInjectionAccumulationPassesRootSignature = nullptr;
		ER_RHI_GPURootSignature* mCompositePassRootSignature = nullptr;

		ER_RHI_GPUShader* mInjectionCS = nullptr;
		std::string mInjectionPassPSOName = "ER_RHI_GPUPipelineStateObject: Volumetric Fog - Injection";

		ER_RHI_GPUShader* mAccumulationCS = nullptr;
		std::string mAccumulationPassPSOName = "ER_RHI_GPUPipelineStateObject: Volumetric Fog - Accumulation";

		ER_RHI_GPUShader* mCompositePS = nullptr;
		std::string mCompositePassPSOName = "ER_RHI_GPUPipelineStateObject: Volumetric Fog - Composite";

		XMMATRIX mPrevViewProj;

		VolumetricFogQuality mCurrentQuality;

		UINT mCurrentVoxelVolumeSizeX = 0;
		UINT mCurrentVoxelVolumeSizeY = 0;

		float mAnisotropy = 0.05f;
		float mDensity = 0.350f;
		float mStrength = 2.0f;
		float mThicknessFactor = 0.01f;
		float mBlendingWithSceneColorFactor = 1.0f;
		float mPreviousFrameBlendFactor = 0.95f;

		float mCustomNearPlane = 0.5f;
		float mCustomFarPlane = 1000.0f;

		bool mCurrentTexture3DRead = false;

		bool mEnabled = true;
		bool mShowDebug = false;
	};

}