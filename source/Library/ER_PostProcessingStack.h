#pragma once
#include "Common.h"
#include "ER_Core.h"
#include "ER_CoreTime.h"
#include "ER_GPUTexture.h"
#include "ConstantBuffer.h"

namespace Library
{
	class ER_Camera;
	class DirectionalLight;
	class ER_QuadRenderer;
	class ER_GBuffer;
	class ER_VolumetricClouds;
	class ER_VolumetricFog;

	namespace PostEffectsCBuffers
	{
		struct FXAACB
		{
			XMFLOAT2 ScreenDimensions;
		};	
		struct VignetteCB
		{
			XMFLOAT2 RadiusSoftness;
		};
		struct LinearFogCB
		{
			XMFLOAT4 FogColor;
			float FogNear;
			float FogFar;
			float FogDensity;
		};
		struct SSRCB
		{
			XMMATRIX InvProjMatrix;
			XMMATRIX InvViewMatrix;
			XMMATRIX ViewMatrix;
			XMMATRIX ProjMatrix;
			XMFLOAT4 CameraPosition;
			float StepSize;
			float MaxThickness;
			float Time;
			int MaxRayCount;
		};
		struct SSSCB
		{
			XMFLOAT4 SSSStrengthWidthDir;
			float CameraFOV;
		};
	}

	class ER_PostProcessingStack
	{
	public:
		ER_PostProcessingStack(ER_Core& pCore, ER_Camera& pCamera);
		~ER_PostProcessingStack();

		void Initialize(bool pTonemap, bool pMotionBlur, bool pColorGrading, bool pVignette, bool pFXAA, bool pSSR = true, bool pFog = false, bool pLightShafts = false);
	
		void Begin(ER_GPUTexture* aInitialRT, ER_GPUTexture* aDepthTarget);
		void End(ER_GPUTexture* aResolveRT = nullptr);

		void DrawEffects(const ER_CoreTime& gameTime, ER_QuadRenderer* quad, ER_GBuffer* gbuffer, 
			ER_VolumetricClouds* aVolumetricClouds = nullptr, ER_VolumetricFog* aVolumetricFog = nullptr);

		void Update();

		void SetDirectionalLight(const DirectionalLight* pLight) { light = pLight; }
		void Config() { mShowDebug = !mShowDebug; }

		bool isWindowOpened = false;

	private:
		void PrepareDrawingTonemapping(ER_GPUTexture* aInputTexture);
		void PrepareDrawingSSR(const ER_CoreTime& gameTime, ER_GPUTexture* aInputTexture, ER_GBuffer* gbuffer);
		void PrepareDrawingSSS(const ER_CoreTime& gameTime, ER_GPUTexture* aInputTexture, ER_GBuffer* gbuffer, bool verticalPass);
		void PrepareDrawingLinearFog(ER_GPUTexture* aInputTexture);
		void PrepareDrawingColorGrading(ER_GPUTexture* aInputTexture);
		void PrepareDrawingVignette(ER_GPUTexture* aInputTexture);
		void PrepareDrawingFXAA(ER_GPUTexture* aInputTexture);
	
		void ShowPostProcessingWindow();

		ER_Core& game;
		ER_Camera& camera;
		const DirectionalLight* light;

		// Tonemap
		ER_GPUTexture* mTonemappingRT = nullptr;
		ID3D11PixelShader* mTonemappingPS = nullptr;
		bool mUseTonemap = true;

		// SSR
		ER_GPUTexture* mSSRRT = nullptr;
		ConstantBuffer<PostEffectsCBuffers::SSRCB> mSSRConstantBuffer;
		ID3D11PixelShader* mSSRPS = nullptr;
		bool mUseSSR = false;
		int mSSRRayCount = 50;
		float mSSRStepSize = 0.741f;
		float mSSRMaxThickness = 0.00021f;

		// SSS
		ER_GPUTexture* mSSSRT = nullptr;
		bool mUseSSS = true;
		ID3D11PixelShader* mSSSPS = nullptr;
		ConstantBuffer<PostEffectsCBuffers::SSSCB> mSSSConstantBuffer;

		// Linear Fog
		ER_GPUTexture* mLinearFogRT = nullptr;
		ConstantBuffer<PostEffectsCBuffers::LinearFogCB> mLinearFogConstantBuffer;
		ID3D11PixelShader* mLinearFogPS = nullptr;
		bool mUseLinearFog = false;
		float mLinearFogColor[3] = { 166.0f / 255.0f, 188.0f / 255.0f, 196.0f / 255.0f };
		float mLinearFogDensity = 730.0f;
		float mLinearFogNearZ = 0.0f;
		float mLinearFogFarZ = 0.0f;

		ER_GPUTexture* mVolumetricFogRT = nullptr;

		// LUT Color Grading
		ER_GPUTexture* mColorGradingRT = nullptr;
		ID3D11ShaderResourceView* mLUTs[3];
		ID3D11PixelShader* mColorGradingPS = nullptr;
		int mColorGradingCurrentLUTIndex = 2;
		bool mUseColorGrading = true;

		// FXAA
		ER_GPUTexture* mFXAART = nullptr;
		ConstantBuffer<PostEffectsCBuffers::FXAACB> mFXAAConstantBuffer;
		ID3D11PixelShader* mFXAAPS = nullptr;
		bool mUseFXAA = true;

		// Vignette
		ER_GPUTexture* mVignetteRT = nullptr;
		ConstantBuffer<PostEffectsCBuffers::VignetteCB> mVignetteConstantBuffer;
		ID3D11PixelShader* mVignettePS = nullptr;
		float mVignetteRadius = 0.75f;
		float mVignetteSoftness = 0.5f;
		bool mUseVignette = true;

		ID3D11PixelShader* mFinalResolvePS = nullptr;

		// just pointers to RTs (not allocated in this system)
		ER_GPUTexture* mRenderTargetBeforeResolve = nullptr; 
		ER_GPUTexture* mRenderTargetBeforePostProcessingPasses = nullptr;
		ER_GPUTexture* mDepthTarget = nullptr;

		bool mShowDebug = false;
	};
}