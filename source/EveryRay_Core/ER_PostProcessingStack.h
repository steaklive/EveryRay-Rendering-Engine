#pragma once
#include "Common.h"
#include "ER_Core.h"
#include "ER_CoreTime.h"

namespace EveryRay_Core
{
	class ER_Camera;
	class ER_DirectionalLight;
	class ER_QuadRenderer;
	class ER_GBuffer;
	class ER_VolumetricClouds;
	class ER_VolumetricFog;

	namespace PostEffectsCBuffers
	{
		struct ER_ALIGN16 FXAACB
		{
			XMFLOAT2 ScreenDimensions;
		};	
		struct ER_ALIGN16 VignetteCB
		{
			XMFLOAT2 RadiusSoftness;
		};
		struct ER_ALIGN16 LinearFogCB
		{
			XMFLOAT4 FogColor;
			float FogNear;
			float FogFar;
			float FogDensity;
		};
		struct ER_ALIGN16 SSRCB
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
		struct ER_ALIGN16 SSSCB
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
	
		void Begin(ER_RHI_GPUTexture* aInitialRT, ER_RHI_GPUTexture* aDepthTarget);
		void End(ER_RHI_GPUTexture* aResolveRT = nullptr);

		void DrawEffects(const ER_CoreTime& gameTime, ER_QuadRenderer* quad, ER_GBuffer* gbuffer, 
			ER_VolumetricClouds* aVolumetricClouds = nullptr, ER_VolumetricFog* aVolumetricFog = nullptr);

		void Update();

		void SetDirectionalLight(const ER_DirectionalLight* pLight) { light = pLight; }
		void Config() { mShowDebug = !mShowDebug; }

		bool isWindowOpened = false;

	private:
		void PrepareDrawingTonemapping(ER_RHI_GPUTexture* aInputTexture);
		void PrepareDrawingSSR(const ER_CoreTime& gameTime, ER_RHI_GPUTexture* aInputTexture, ER_GBuffer* gbuffer);
		void PrepareDrawingSSS(const ER_CoreTime& gameTime, ER_RHI_GPUTexture* aInputTexture, ER_GBuffer* gbuffer, bool verticalPass);
		void PrepareDrawingLinearFog(ER_RHI_GPUTexture* aInputTexture);
		void PrepareDrawingColorGrading(ER_RHI_GPUTexture* aInputTexture);
		void PrepareDrawingVignette(ER_RHI_GPUTexture* aInputTexture);
		void PrepareDrawingFXAA(ER_RHI_GPUTexture* aInputTexture);
	
		void ShowPostProcessingWindow();

		ER_Core& mCore;
		ER_Camera& camera;
		const ER_DirectionalLight* light;

		// Tonemap
		ER_RHI_GPUTexture* mTonemappingRT = nullptr;
		ER_RHI_GPUShader* mTonemappingPS = nullptr;
		bool mUseTonemap = true;

		// SSR
		ER_RHI_GPUTexture* mSSRRT = nullptr;
		ER_RHI_GPUConstantBuffer<PostEffectsCBuffers::SSRCB> mSSRConstantBuffer;
		ER_RHI_GPUShader* mSSRPS = nullptr;
		bool mUseSSR = false;
		int mSSRRayCount = 50;
		float mSSRStepSize = 0.741f;
		float mSSRMaxThickness = 0.00021f;

		// SSS
		ER_RHI_GPUTexture* mSSSRT = nullptr;
		bool mUseSSS = true;
		ER_RHI_GPUShader* mSSSPS = nullptr;
		ER_RHI_GPUConstantBuffer<PostEffectsCBuffers::SSSCB> mSSSConstantBuffer;

		// Linear Fog
		ER_RHI_GPUTexture* mLinearFogRT = nullptr;
		ER_RHI_GPUConstantBuffer<PostEffectsCBuffers::LinearFogCB> mLinearFogConstantBuffer;
		ER_RHI_GPUShader* mLinearFogPS = nullptr;
		bool mUseLinearFog = false;
		float mLinearFogColor[3] = { 166.0f / 255.0f, 188.0f / 255.0f, 196.0f / 255.0f };
		float mLinearFogDensity = 730.0f;
		float mLinearFogNearZ = 0.0f;
		float mLinearFogFarZ = 0.0f;

		ER_RHI_GPUTexture* mVolumetricFogRT = nullptr;

		// LUT Color Grading
		ER_RHI_GPUTexture* mColorGradingRT = nullptr;
		ER_RHI_GPUTexture* mLUTs[3] = { nullptr, nullptr, nullptr };
		ER_RHI_GPUShader* mColorGradingPS = nullptr;
		int mColorGradingCurrentLUTIndex = 2;
		bool mUseColorGrading = true;

		// FXAA
		ER_RHI_GPUTexture* mFXAART = nullptr;
		ER_RHI_GPUConstantBuffer<PostEffectsCBuffers::FXAACB> mFXAAConstantBuffer;
		ER_RHI_GPUShader* mFXAAPS = nullptr;
		bool mUseFXAA = true;

		// Vignette
		ER_RHI_GPUTexture* mVignetteRT = nullptr;
		ER_RHI_GPUConstantBuffer<PostEffectsCBuffers::VignetteCB> mVignetteConstantBuffer;
		ER_RHI_GPUShader* mVignettePS = nullptr;
		float mVignetteRadius = 0.75f;
		float mVignetteSoftness = 0.5f;
		bool mUseVignette = true;

		ER_RHI_GPUShader* mFinalResolvePS = nullptr;

		// just pointers to RTs (not allocated in this system)
		ER_RHI_GPUTexture* mRenderTargetBeforeResolve = nullptr;
		ER_RHI_GPUTexture* mRenderTargetBeforePostProcessingPasses = nullptr;
		ER_RHI_GPUTexture* mDepthTarget = nullptr;

		bool mShowDebug = false;
	};
}