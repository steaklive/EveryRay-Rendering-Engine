#pragma once
#include "Common.h"
#include "ConstantBuffer.h"
#include "ER_GPUTexture.h"
#include "DepthTarget.h"
#include "GameComponent.h"

namespace Library
{
	class FullScreenRenderTarget;
	class DirectionalLight;
	class GameTime;
	class Camera;
	class ER_Skybox;

	namespace VolumetricCloudsCBufferData {
		struct FrameCB
		{
			XMMATRIX	InvProj;
			XMMATRIX	InvView;
			XMVECTOR	LightDir;
			XMVECTOR	LightCol;
			XMVECTOR	CameraPos;
			XMFLOAT2	Resolution;
		};

		struct CloudsCB
		{
			XMVECTOR AmbientColor;
			XMVECTOR WindDir;
			float WindSpeed;
			float Time;
			float Crispiness;
			float Curliness;
			float Coverage;
			float Absorption;
			float CloudsBottomHeight;
			float CloudsTopHeight;
			float DensityFactor;
		};
	}

	class ER_VolumetricClouds : public GameComponent
	{
	public:
		ER_VolumetricClouds(Game& game, Camera& camera, DirectionalLight& light, ER_Skybox& skybox);
		~ER_VolumetricClouds();

		void Initialize(ER_GPUTexture* aIlluminationColor, DepthTarget* aIlluminationDepth);

		void Draw(const GameTime& gametime);
		void Update(const GameTime& gameTime);
		void Config() { mShowDebug = !mShowDebug; }
	private:
		void UpdateImGui();

		Camera& mCamera;
		DirectionalLight& mDirectionalLight;
		ER_Skybox& mSkybox;
		
		ConstantBuffer<VolumetricCloudsCBufferData::FrameCB> mFrameConstantBuffer;
		ConstantBuffer<VolumetricCloudsCBufferData::CloudsCB> mCloudsConstantBuffer;

		ER_GPUTexture* mIlluminationResultRT = nullptr; // not allocated here, just a pointer
		DepthTarget* mIlluminationResultDepthTarget = nullptr; // not allocated here, just a pointer

		ER_GPUTexture* mSkyRT = nullptr;
		ER_GPUTexture* mSkyAndSunRT = nullptr;
		ER_GPUTexture* mMainRT = nullptr;
		FullScreenRenderTarget* mBlurRT = nullptr;

		ID3D11ComputeShader* mMainCS = nullptr;
		ID3D11PixelShader* mCompositePS = nullptr;
		ID3D11PixelShader* mBlurPS = nullptr;

		ID3D11ShaderResourceView* mCloudTextureSRV = nullptr;
		ID3D11ShaderResourceView* mWeatherTextureSRV = nullptr;
		ID3D11ShaderResourceView* mWorleyTextureSRV = nullptr;

		ID3D11SamplerState* mCloudSS = nullptr;
		ID3D11SamplerState* mWeatherSS = nullptr;

		float mCrispiness = 43.0f;
		float mCurliness = 1.1f;
		float mCoverage = 0.305f;
		float mAmbientColor[3] = { 102.0f / 255.0f, 104.0f / 255.0f, 105.0f / 255.0f };
		float mWindSpeedMultiplier = 175.0f;
		float mLightAbsorption = 0.003f;
		float mCloudsBottomHeight = 2340.0f;
		float mCloudsTopHeight = 16400.0f;
		float mDensityFactor = 0.012f;

		bool mEnabled = true;
		bool mShowDebug = false;
	};
}