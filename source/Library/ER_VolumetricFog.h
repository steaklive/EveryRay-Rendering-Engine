#pragma once

#include "Common.h"
#include "ConstantBuffer.h"
#include "ER_GPUTexture.h"
#include "DepthTarget.h"
#include "GameComponent.h"

namespace Library
{
	class DirectionalLight;
	class GameTime;
	class Camera;
	class ER_Skybox;
	class ER_ShadowMapper;

	namespace VolumetricFogCBufferData {
		struct MainCB
		{
			XMMATRIX InvViewProj;
			XMMATRIX ShadowMatrix;
			XMFLOAT4 SunDirection;
			XMFLOAT4 SunColor;
			XMFLOAT4 CameraPosition;
			XMFLOAT4 CameraNearFar;
			float Anisotropy;
			float Density;
			float Strength;
		};	
		struct CompositeCB
		{
			XMMATRIX ViewProj;
			XMFLOAT4 CameraNearFar;
		};
	}

	class ER_VolumetricFog : public GameComponent
	{
	public:
		ER_VolumetricFog(Game& game, const DirectionalLight& aLight, const ER_ShadowMapper& aShadowMapper);
		~ER_VolumetricFog();
	
		void Initialize();
		void Draw();
		void Composite(ER_GPUTexture* aInputColorTexture, ER_GPUTexture* aGbufferWorldPos);
		void Update(const GameTime& gameTime);
		void Config() { mShowDebug = !mShowDebug; }
		bool IsEnabled() { return mEnabled; }

		ER_GPUTexture* GetVoxelFogTexture() { return mFinalVoxelAccumulationTexture3D; }
	private:
		void ComputeInjection();
		void ComputeAccumulation();
		void UpdateImGui();

		const ER_ShadowMapper& mShadowMapper;
		const DirectionalLight& mDirectionalLight;

		ER_GPUTexture* mTempVoxelInjectionTexture3D[2] = { nullptr, nullptr }; //read-write
		ER_GPUTexture* mFinalVoxelAccumulationTexture3D = nullptr;
		ER_GPUTexture* mBlueNoiseTexture = nullptr;

		ConstantBuffer<VolumetricFogCBufferData::MainCB> mMainConstantBuffer;
		ConstantBuffer<VolumetricFogCBufferData::CompositeCB> mCompositeConstantBuffer;

		ID3D11ComputeShader* mInjectionCS = nullptr;
		ID3D11ComputeShader* mAccumulationCS = nullptr;
		ID3D11PixelShader* mCompositePS = nullptr;

		float mAnisotropy = 0.05f;
		float mDensity = 0.350f;
		float mStrength = 2.0f;

		bool mCurrentTexture3DRead = false;

		bool mEnabled = true;
		bool mShowDebug = false;
	};

}