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

	namespace VolumetricFogCBufferData {
		struct ER_ALIGN_GPU_BUFFER MainCB
		{
			XMMATRIX InvViewProj;
			XMMATRIX PrevViewProj;
			XMMATRIX ShadowMatrix;
			XMFLOAT4 SunDirection;
			XMFLOAT4 SunColor;
			XMFLOAT4 CameraPosition;
			XMFLOAT4 CameraNearFar;
			float Anisotropy;
			float Density;
			float Strength;
			float ThicknessFactor;
			float AmbientIntensity;
			float PreviousFrameBlend;
			float FrameIndex;
		};	
		struct ER_ALIGN_GPU_BUFFER CompositeCB
		{
			XMMATRIX ViewProj;
			XMFLOAT4 CameraNearFar;
			float BlendingWithSceneColorFactor;
		};
	}

	class ER_VolumetricFog : public ER_CoreComponent
	{
	public:
		ER_VolumetricFog(ER_Core& game, const ER_DirectionalLight& aLight, const ER_ShadowMapper& aShadowMapper);
		~ER_VolumetricFog();
	
		void Initialize();
		void Draw();
		void Composite(ER_RHI_GPUTexture* aRT, ER_RHI_GPUTexture* aInputColorTexture, ER_RHI_GPUTexture* aGbufferWorldPos);
		void Update(const ER_CoreTime& gameTime);
		void Config() { mShowDebug = !mShowDebug; }
		bool IsEnabled() { return mEnabled; }
		void SetEnabled(bool val) { mEnabled = val; }

		ER_RHI_GPUTexture* GetVoxelFogTexture() { return mFinalVoxelAccumulationTexture3D; }
	private:
		void ComputeInjection();
		void ComputeAccumulation();
		void UpdateImGui();

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

		float mAnisotropy = 0.05f;
		float mDensity = 0.350f;
		float mStrength = 2.0f;
		float mThicknessFactor = 0.01f;
		float mAmbientIntensity = 0.0f;
		float mBlendingWithSceneColorFactor = 1.0f;
		float mPreviousFrameBlendFactor = 0.05f;

		bool mCurrentTexture3DRead = false;

		bool mEnabled = true;
		bool mShowDebug = false;
	};

}