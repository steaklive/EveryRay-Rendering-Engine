#pragma once

#include "Common.h"
#include "ConstantBuffer.h"
#include "ER_CoreComponent.h"
#include "RHI/ER_RHI.h"

namespace Library
{
	class DirectionalLight;
	class ER_CoreTime;
	class ER_Camera;
	class ER_Skybox;
	class ER_ShadowMapper;

	namespace VolumetricFogCBufferData {
		struct MainCB
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
		};	
		struct CompositeCB
		{
			XMMATRIX ViewProj;
			XMFLOAT4 CameraNearFar;
			float BlendingWithSceneColorFactor;
		};
	}

	class ER_VolumetricFog : public ER_CoreComponent
	{
	public:
		ER_VolumetricFog(ER_Core& game, const DirectionalLight& aLight, const ER_ShadowMapper& aShadowMapper);
		~ER_VolumetricFog();
	
		void Initialize();
		void Draw();
		void Composite(ER_RHI_GPUTexture* aInputColorTexture, ER_RHI_GPUTexture* aGbufferWorldPos);
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
		const DirectionalLight& mDirectionalLight;

		ER_RHI_GPUTexture* mTempVoxelInjectionTexture3D[2] = { nullptr, nullptr }; //read-write
		ER_RHI_GPUTexture* mFinalVoxelAccumulationTexture3D = nullptr;
		ER_RHI_GPUTexture* mBlueNoiseTexture = nullptr;

		ConstantBuffer<VolumetricFogCBufferData::MainCB> mMainConstantBuffer;
		ConstantBuffer<VolumetricFogCBufferData::CompositeCB> mCompositeConstantBuffer;

		ER_RHI_GPUShader* mInjectionCS = nullptr;
		ER_RHI_GPUShader* mAccumulationCS = nullptr;
		ER_RHI_GPUShader* mCompositePS = nullptr;

		XMMATRIX mPrevViewProj;

		float mAnisotropy = 0.05f;
		float mDensity = 0.350f;
		float mStrength = 2.0f;
		float mThicknessFactor = 0.01f;
		float mAmbientIntensity = 0.1f;
		float mBlendingWithSceneColorFactor = 1.0f;
		float mPreviousFrameBlendFactor = 0.05f;

		bool mCurrentTexture3DRead = false;

		bool mEnabled = true;
		bool mShowDebug = false;
	};

}