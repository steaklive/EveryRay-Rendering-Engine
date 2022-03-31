#pragma once
#include "Common.h"
#include "Game.h"
#include "Camera.h"
#include "GameTime.h"
#include "ER_GPUTexture.h"
#include "DepthTarget.h"
#include "ConstantBuffer.h"

namespace Library
{
	class Effect;
	class FullScreenRenderTarget;
	class FullScreenQuad;
	class DirectionalLight;
	class ER_QuadRenderer;
}

namespace Rendering
{
	class ScreenSpaceReflectionsMaterial;
	class LightShaftsMaterial;

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
	}

	namespace EffectElements
	{
		struct LightShaftsEffect
		{
			LightShaftsEffect() :
				Material(nullptr), Quad(nullptr), OutputTexture(nullptr), DepthTexture(nullptr)
			{}

			~LightShaftsEffect()
			{
				DeleteObject(Material);
				DeleteObject(Quad);
				ReleaseObject(OutputTexture);
				ReleaseObject(DepthTexture);
			}

			LightShaftsMaterial* Material;
			FullScreenQuad* Quad;
			ID3D11ShaderResourceView* OutputTexture;
			ID3D11ShaderResourceView* DepthTexture;
			bool isActive = true;
			bool isForceEnable = true;
			bool isSunOnScreen = false;

			float decay = 0.995f;
			float weight = 6.65f;
			float exposure = 0.011f;
			float density = 0.94f;
			float maxSunDistanceDelta = 10.0f;
			float intensity = 0.1f;
		};

		struct SSREffect
		{
			SSREffect() :
				Material(nullptr), Quad(nullptr), InputColor(nullptr)
			{}

			~SSREffect()
			{
				DeleteObject(Material);
				DeleteObject(Quad);
				ReleaseObject(InputColor);

			}

			ScreenSpaceReflectionsMaterial* Material;
			FullScreenQuad* Quad;
			ID3D11ShaderResourceView* InputColor;

			bool isActive = true;
			int rayCount = 50;
			float stepSize = 0.741f;
			float maxThickness = 0.00021f;
		};
	}

	class PostProcessingStack
	{
	public:

		PostProcessingStack() = delete;
		PostProcessingStack(Game& pGame, Camera& pCamera);
		~PostProcessingStack();

		void UpdateSSRMaterial(ID3D11ShaderResourceView* normal, ID3D11ShaderResourceView* depth, ID3D11ShaderResourceView* extra, float time);
		void UpdateLightShaftsMaterial();

		void Initialize(bool pTonemap, bool pMotionBlur, bool pColorGrading, bool pVignette, bool pFXAA, bool pSSR = true, bool pFog = false, bool pLightShafts = false);
	
		void Begin(ER_GPUTexture* aInitialRT, DepthTarget* aDepthTarget);
		void End(ER_GPUTexture* aResolveRT = nullptr);
		
		void BeginRenderingToExtraRT(bool clear);
		void EndRenderingToExtraRT();

		void DrawEffects(const GameTime& gameTime, ER_QuadRenderer* quad);

		void Update();

		void SetDirectionalLight(const DirectionalLight* pLight) { light = pLight; }
		void SetSunOcclusionSRV(ID3D11ShaderResourceView* srv) { mSunOcclusionSRV = srv; }
		void SetSunNDCPos(XMFLOAT2 pos) { mSunNDCPos = pos; }

		void Config() { mShowDebug = !mShowDebug; }

		ID3D11ShaderResourceView* GetExtraColorSRV();

		bool isWindowOpened = false;

	private:
		void PrepareDrawingLinearFog(ER_GPUTexture* aInputTexture);
		void PrepareDrawingColorGrading(ER_GPUTexture* aInputTexture);
		void PrepareDrawingVignette(ER_GPUTexture* aInputTexture);
		void PrepareDrawingFXAA(ER_GPUTexture* aInputTexture);
	
		void ShowPostProcessingWindow();

		Game& game;
		Camera& camera;
		const DirectionalLight* light;

		EffectElements::SSREffect* mSSREffect;
		EffectElements::LightShaftsEffect* mLightShaftsEffect;
		
		//TODO change to custom render targets
		FullScreenRenderTarget* mSSRRenderTarget;
		FullScreenRenderTarget* mLightShaftsRenderTarget;
		FullScreenRenderTarget* mExtraRenderTarget;

		ID3D11ShaderResourceView* mSunOcclusionSRV = nullptr;

		bool mSSRLoaded = false;
		bool mLightShaftsLoaded = false;

		ER_GPUTexture* mLinearFogRT = nullptr;
		ConstantBuffer<PostEffectsCBuffers::LinearFogCB> mLinearFogConstantBuffer;
		ID3D11PixelShader* mLinearFogPS = nullptr;
		bool mUseLinearFog = true;
		float mLinearFogColor[3] = { 166.0f / 255.0f, 188.0f / 255.0f, 196.0f / 255.0f };
		float mLinearFogDensity = 730.0f;
		float mLinearFogNearZ = 0.0f;
		float mLinearFogFarZ = 0.0f;

		ER_GPUTexture* mColorGradingRT = nullptr;
		ID3D11ShaderResourceView* mLUTs[3];
		ID3D11PixelShader* mColorGradingPS = nullptr;
		int mColorGradingCurrentLUTIndex = 2;
		bool mUseColorGrading = false;

		ER_GPUTexture* mFXAART = nullptr;
		ConstantBuffer<PostEffectsCBuffers::FXAACB> mFXAAConstantBuffer;
		ID3D11PixelShader* mFXAAPS = nullptr;
		bool mUseFXAA = true;

		ER_GPUTexture* mVignetteRT = nullptr;
		ConstantBuffer<PostEffectsCBuffers::VignetteCB> mVignetteConstantBuffer;
		ID3D11PixelShader* mVignettePS = nullptr;
		float mVignetteRadius = 0.75f;
		float mVignetteSoftness = 0.5f;
		bool mUseVignette = true;

		ID3D11PixelShader* mFinalResolvePS = nullptr;

		ER_GPUTexture* mFinalTargetBeforeResolve = nullptr; // just a pointer
		ER_GPUTexture* mFirstTargetBeforePostProcessingPasses = nullptr; // just a pointer
		DepthTarget* mDepthTarget = nullptr; //just a pointer

		XMFLOAT2 mSunNDCPos;
		bool mShowDebug = false;
	};
}